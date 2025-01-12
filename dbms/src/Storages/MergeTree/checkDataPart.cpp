#include <algorithm>
#include <optional>

#include <Poco/File.h>
#include <Poco/DirectoryIterator.h>

#include <Storages/MergeTree/MergeTreeIndexGranularity.h>
#include <Storages/MergeTree/checkDataPart.h>
#include <DataStreams/MarkInCompressedFile.h>
#include <Compression/CompressedReadBuffer.h>
#include <IO/HashingReadBuffer.h>
#include <Common/CurrentMetrics.h>


namespace CurrentMetrics
{
    extern const Metric ReplicatedChecks;
}

namespace DB
{

namespace ErrorCodes
{
    extern const int CORRUPTED_DATA;
    extern const int LOGICAL_ERROR;
    extern const int INCORRECT_MARK;
    extern const int EMPTY_LIST_OF_COLUMNS_PASSED;
}


namespace
{

/** To read and checksum single stream (a pair of .bin, .mrk files) for a single column or secondary index.
  */
class Stream
{
public:
    String base_name;
    String bin_file_extension;
    String mrk_file_extension;
    String bin_file_path;
    String mrk_file_path;
private:
    const MergeTreeIndexGranularity & index_granularity;
    ReadBufferFromFile file_buf;
    HashingReadBuffer compressed_hashing_buf;
    CompressedReadBuffer uncompressing_buf;
    size_t mark_position = 0;
public:
    HashingReadBuffer uncompressed_hashing_buf;

private:
    ReadBufferFromFile mrk_file_buf;

    std::pair<MarkInCompressedFile, size_t> readMarkFromFile()
    {
        size_t mrk_rows;
        MarkInCompressedFile mrk_mark;
        readIntBinary(mrk_mark.offset_in_compressed_file, mrk_hashing_buf);
        readIntBinary(mrk_mark.offset_in_decompressed_block, mrk_hashing_buf);
        if (mrk_file_extension == ".mrk2")
            readIntBinary(mrk_rows, mrk_hashing_buf);
        else
            mrk_rows = index_granularity.getMarkRows(mark_position);

        return {mrk_mark, mrk_rows};
    }
public:
    HashingReadBuffer mrk_hashing_buf;

        Stream(
            const String & path,
            const String & base_name_,
            const String & bin_file_extension_,
            const String & mrk_file_extension_,
            const MergeTreeIndexGranularity & index_granularity_)
        : base_name(base_name_)
        , bin_file_extension(bin_file_extension_)
        , mrk_file_extension(mrk_file_extension_)
        , bin_file_path(path + base_name + bin_file_extension)
        , mrk_file_path(path + base_name + mrk_file_extension)
        , index_granularity(index_granularity_)
        , file_buf(bin_file_path)
        , compressed_hashing_buf(file_buf)
        , uncompressing_buf(compressed_hashing_buf)
        , uncompressed_hashing_buf(uncompressing_buf)
        , mrk_file_buf(mrk_file_path)
        , mrk_hashing_buf(mrk_file_buf)
    {}

    void assertMark(bool only_read=false)
    {

        auto [mrk_mark, mrk_rows] = readMarkFromFile();
        bool has_alternative_mark = false;
        MarkInCompressedFile alternative_data_mark = {};
        MarkInCompressedFile data_mark = {};

        /// If the mark should be exactly at the border of blocks, we can also use a mark pointing to the end of previous block,
        ///  and the beginning of next.
        if (!uncompressed_hashing_buf.hasPendingData())
        {
            /// Get a mark pointing to the end of previous block.
            has_alternative_mark = true;
            alternative_data_mark.offset_in_compressed_file = compressed_hashing_buf.count() - uncompressing_buf.getSizeCompressed();
            alternative_data_mark.offset_in_decompressed_block = uncompressed_hashing_buf.offset();

            if (mrk_mark == alternative_data_mark)
            {
                mark_position++;
                return;
            }

            uncompressed_hashing_buf.next();

            /// At the end of file `compressed_hashing_buf.count()` points to the end of the file even before `calling next()`,
            ///  and the check you just performed does not work correctly. For simplicity, we will not check the last mark.
            if (uncompressed_hashing_buf.eof())
            {
                mark_position++;
                return;
            }
        }

        data_mark.offset_in_compressed_file = compressed_hashing_buf.count() - uncompressing_buf.getSizeCompressed();
        data_mark.offset_in_decompressed_block = uncompressed_hashing_buf.offset();

        if (!only_read && (mrk_mark != data_mark || mrk_rows != index_granularity.getMarkRows(mark_position)))
            throw Exception("Incorrect mark: " + data_mark.toStringWithRows(index_granularity.getMarkRows(mark_position)) +
                (has_alternative_mark ? " or " + alternative_data_mark.toString() : "") + " in data, " +
                mrk_mark.toStringWithRows(mrk_rows) + " in " + mrk_file_path + " file", ErrorCodes::INCORRECT_MARK);

        mark_position++;
    }

    void assertEnd()
    {
        if (!uncompressed_hashing_buf.eof())
            throw Exception("EOF expected in " + bin_file_path + " file"
                + " at position "
                + toString(compressed_hashing_buf.count()) + " (compressed), "
                + toString(uncompressed_hashing_buf.count()) + " (uncompressed)", ErrorCodes::CORRUPTED_DATA);

        if (index_granularity.hasFinalMark())
        {
            auto final_mark_rows = readMarkFromFile().second;
            if (final_mark_rows != 0)
                throw Exception("Incorrect final mark at the end of " + mrk_file_path + " expected 0 rows, got " + toString(final_mark_rows), ErrorCodes::CORRUPTED_DATA);
        }
        if (!mrk_hashing_buf.eof())
            throw Exception("EOF expected in " + mrk_file_path + " file"
                + " at position "
                + toString(mrk_hashing_buf.count()), ErrorCodes::CORRUPTED_DATA);
    }

    void saveChecksums(MergeTreeData::DataPart::Checksums & checksums)
    {
        checksums.files[base_name + bin_file_extension] = MergeTreeData::DataPart::Checksums::Checksum(
            compressed_hashing_buf.count(), compressed_hashing_buf.getHash(),
            uncompressed_hashing_buf.count(), uncompressed_hashing_buf.getHash());

        checksums.files[base_name + mrk_file_extension] = MergeTreeData::DataPart::Checksums::Checksum(
            mrk_hashing_buf.count(), mrk_hashing_buf.getHash());
    }
};

}


MergeTreeData::DataPart::Checksums checkDataPart(
    const String & full_path,
    const MergeTreeIndexGranularity & adaptive_index_granularity,
    const String & mrk_file_extension,
    bool require_checksums,
    const DataTypes & primary_key_data_types,
    const MergeTreeIndices & indices,
    std::function<bool()> is_cancelled)
{
    Logger * log = &Logger::get("checkDataPart");

    /** Responsibility:
      * - read list of columns from columns.txt;
      * - read checksums if exist;
      * - read (and validate checksum) of primary.idx; obtain number of marks;
      * - read data files and marks for each stream of each column; calculate and validate checksums;
      * - check that there are the same number of rows in each column;
      * - check that all marks files have the same size;
      */

    CurrentMetrics::Increment metric_increment{CurrentMetrics::ReplicatedChecks};

    String path = full_path;
    if (!path.empty() && path.back() != '/')
        path += "/";

    NamesAndTypesList columns;

    {
        ReadBufferFromFile buf(path + "columns.txt");
        columns.readText(buf);
        assertEOF(buf);
    }

    /// Checksums from file checksums.txt. May be absent. If present, they are subsequently compared with the actual data checksums.
    MergeTreeData::DataPart::Checksums checksums_txt;

    if (require_checksums || Poco::File(path + "checksums.txt").exists())
    {
        ReadBufferFromFile buf(path + "checksums.txt");
        checksums_txt.read(buf);
        assertEOF(buf);
    }

    /// Real checksums based on contents of data. Must correspond to checksums.txt. If not - it means the data is broken.
    MergeTreeData::DataPart::Checksums checksums_data;

    size_t marks_in_primary_key = 0;
    if (!primary_key_data_types.empty())
    {
        ReadBufferFromFile file_buf(path + "primary.idx");
        HashingReadBuffer hashing_buf(file_buf);

        size_t key_size = primary_key_data_types.size();
        MutableColumns tmp_columns(key_size);

        for (size_t j = 0; j < key_size; ++j)
            tmp_columns[j] = primary_key_data_types[j]->createColumn();

        while (!hashing_buf.eof())
        {
            if (is_cancelled())
                return {};

            ++marks_in_primary_key;
            for (size_t j = 0; j < key_size; ++j)
                primary_key_data_types[j]->deserializeBinary(*tmp_columns[j].get(), hashing_buf);
        }

        size_t primary_idx_size = hashing_buf.count();

        checksums_data.files["primary.idx"] = MergeTreeData::DataPart::Checksums::Checksum(primary_idx_size, hashing_buf.getHash());
    }

    /// Optional files count.txt, partition.dat, minmax_*.idx, ttl.txt. Just calculate checksums for existing files.
    Poco::DirectoryIterator dir_end;
    for (Poco::DirectoryIterator dir_it(path); dir_it != dir_end; ++dir_it)
    {
        const String & file_name = dir_it.name();
        if (file_name == "count.txt"
            || file_name == "partition.dat"
            || (startsWith(file_name, "minmax_") && endsWith(file_name, ".idx"))
            || file_name == "ttl.txt")
        {
            ReadBufferFromFile file_buf(dir_it->path());
            HashingReadBuffer hashing_buf(file_buf);
            hashing_buf.tryIgnore(std::numeric_limits<size_t>::max());
            checksums_data.files[file_name] = MergeTreeData::DataPart::Checksums::Checksum(hashing_buf.count(), hashing_buf.getHash());
        }
    }

    if (is_cancelled())
        return {};

    /// If count.txt file exists, use it as source of truth for number of rows. Otherwise just check that all columns have equal amount of rows.
    std::optional<size_t> rows;

    if (Poco::File(path + "count.txt").exists())
    {
        ReadBufferFromFile buf(path + "count.txt");
        size_t count = 0;
        readText(count, buf);
        assertEOF(buf);
        rows = count;
    }

    /// Read and check skip indices.
    for (const auto & index : indices)
    {
        Stream stream(path, index->getFileName(), ".idx", mrk_file_extension, adaptive_index_granularity);
        size_t mark_num = 0;

        while (!stream.uncompressed_hashing_buf.eof())
        {
            if (stream.mrk_hashing_buf.eof())
                throw Exception("Unexpected end of mrk file while reading index " + index->name,
                                ErrorCodes::CORRUPTED_DATA);
            try
            {
                stream.assertMark();
            }
            catch (Exception & e)
            {
                e.addMessage("Cannot read mark " + toString(mark_num)
                             + " in file " + stream.mrk_file_path
                             + ", mrk file offset: " + toString(stream.mrk_hashing_buf.count()));
                throw;
            }
            try
            {
                index->createIndexGranule()->deserializeBinary(stream.uncompressed_hashing_buf);
            }
            catch (Exception & e)
            {
                e.addMessage("Cannot read granule " + toString(mark_num)
                          + " in file " + stream.bin_file_path
                          + ", mrk file offset: " + toString(stream.mrk_hashing_buf.count()));
                throw;
            }
            ++mark_num;
            if (is_cancelled())
                return {};
        }

        stream.assertEnd();
        stream.saveChecksums(checksums_data);
    }

    /// Read all columns, calculate checksums and validate marks.
    for (const NameAndTypePair & name_type : columns)
    {
        LOG_DEBUG(log, "Checking column " + name_type.name + " in " + path);

        std::map<String, Stream> streams;
        size_t column_size = 0;
        size_t mark_num = 0;

        IDataType::DeserializeBinaryBulkStatePtr state;
        IDataType::DeserializeBinaryBulkSettings settings;
        settings.getter = [&](const IDataType::SubstreamPath & substream_path)
            {
                String file_name = IDataType::getFileNameForStream(name_type.name, substream_path);
                auto & stream = streams.try_emplace(file_name, path, file_name, ".bin", mrk_file_extension, adaptive_index_granularity).first->second;
                return &stream.uncompressed_hashing_buf;
            };

        /// Prefixes have to be read before data because first mark points after prefix
        name_type.type->deserializeBinaryBulkStatePrefix(settings, state);

        while (true)
        {

            /// Check that mark points to current position in file.
            bool marks_eof = false;
            name_type.type->enumerateStreams([&](const IDataType::SubstreamPath & substream_path)
                {
                    String file_name = IDataType::getFileNameForStream(name_type.name, substream_path);

                    auto & stream = streams.try_emplace(file_name, path, file_name, ".bin", mrk_file_extension, adaptive_index_granularity).first->second;
                    try
                    {
                        /// LowCardinality dictionary column is not read monotonically, so marks maybe inconsistent with
                        /// offset position in file. So we just read data and marks file, but doesn't check marks equality.
                        bool only_read = !substream_path.empty() && substream_path.back().type == IDataType::Substream::DictionaryKeys;
                        if (!stream.mrk_hashing_buf.eof())
                            stream.assertMark(only_read);
                        else
                            marks_eof = true;
                    }
                    catch (Exception & e)
                    {
                        e.addMessage("Cannot read mark " + toString(mark_num) + " at row " + toString(column_size)
                            + " in file " + stream.mrk_file_path
                            + ", mrk file offset: " + toString(stream.mrk_hashing_buf.count()));
                        throw;
                    }
                }, settings.path);

            size_t rows_after_mark = adaptive_index_granularity.getMarkRows(mark_num);
            ++mark_num;

            /// Read index_granularity rows from column.
            /// NOTE Shared array sizes of Nested columns are read more than once. That's Ok.

            MutableColumnPtr tmp_column = name_type.type->createColumn();
            name_type.type->deserializeBinaryBulkWithMultipleStreams(*tmp_column, rows_after_mark, settings, state);

            size_t read_size = tmp_column->size();
            column_size += read_size;

            if (read_size < rows_after_mark || mark_num == adaptive_index_granularity.getMarksCount())
                break;
            else if (marks_eof)
                throw Exception("Unexpected end of mrk file while reading column " + name_type.name, ErrorCodes::CORRUPTED_DATA);

            if (is_cancelled())
                return {};
        }

        /// Check that number of rows are equal in each column.
        if (!rows)
            rows = column_size;
        else if (*rows != column_size)
            throw Exception{"Unexpected number of rows in column "
                + name_type.name + " (" + toString(column_size) + ", expected: " + toString(*rows) + ")",
                ErrorCodes::SIZES_OF_COLUMNS_DOESNT_MATCH};

        /// Save checksums for column.
        name_type.type->enumerateStreams([&](const IDataType::SubstreamPath & substream_path)
            {
                String file_name = IDataType::getFileNameForStream(name_type.name, substream_path);
                auto stream_it = streams.find(file_name);
                if (stream_it == streams.end())
                    throw Exception("Logical error: cannot find stream " + file_name, ErrorCodes::LOGICAL_ERROR);

                stream_it->second.assertEnd();
                stream_it->second.saveChecksums(checksums_data);
            }, {});

        if (is_cancelled())
            return {};
    }

    if (!rows)
        throw Exception("No columns in data part", ErrorCodes::EMPTY_LIST_OF_COLUMNS_PASSED);

    if (!primary_key_data_types.empty())
    {
        size_t expected_marks = adaptive_index_granularity.getMarksCount();
        if (expected_marks != marks_in_primary_key)
        {
            throw Exception("Size of primary key doesn't match expected number of marks."
                " Number of rows in columns: " + toString(*rows)
                + ", expected number of marks: " + toString(expected_marks)
                + ", size of primary key: " + toString(marks_in_primary_key),
                ErrorCodes::CORRUPTED_DATA);
        }
    }

    if (require_checksums || !checksums_txt.files.empty())
        checksums_txt.checkEqual(checksums_data, true);

    return checksums_data;
}

MergeTreeData::DataPart::Checksums checkDataPart(
    MergeTreeData::DataPartPtr data_part,
    bool require_checksums,
    const DataTypes & primary_key_data_types,
    const MergeTreeIndices & indices,
    std::function<bool()> is_cancelled)
{
    return checkDataPart(
        data_part->getFullPath(),
        data_part->index_granularity,
        data_part->index_granularity_info.marks_file_extension,
        require_checksums,
        primary_key_data_types,
        indices,
        is_cancelled);
}


}
