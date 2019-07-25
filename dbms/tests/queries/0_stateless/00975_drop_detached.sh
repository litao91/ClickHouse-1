#!/usr/bin/env bash

set -e

CURDIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
. $CURDIR/../shell_config.sh

ch_dir=`${CLICKHOUSE_EXTRACT_CONFIG} -k path`
cur_db=`${CLICKHOUSE_CLIENT} --query "SELECT currentDatabase()"`

$CLICKHOUSE_CLIENT --query="SYSTEM STOP MERGES";
$CLICKHOUSE_CLIENT --query="DROP TABLE IF EXISTS drop_detached";
$CLICKHOUSE_CLIENT --query="CREATE TABLE drop_detached (n UInt64) ENGINE = MergeTree() PARTITION BY intDiv(n, 8) ORDER BY n";
$CLICKHOUSE_CLIENT --query="INSERT INTO drop_detached SELECT number FROM system.numbers WHERE number % 2 = 0 LIMIT 8";
$CLICKHOUSE_CLIENT --query="INSERT INTO drop_detached SELECT number FROM system.numbers WHERE number % 2 = 1 LIMIT 8";

$CLICKHOUSE_CLIENT --query="ALTER TABLE drop_detached DETACH PARTITION 0";
mkdir $ch_dir/data/$cur_db/drop_detached/detached/attaching_0_6_6_0/
mkdir $ch_dir/data/$cur_db/drop_detached/detached/delete_tmp_0_7_7_0/
mkdir $ch_dir/data/$cur_db/drop_detached/detached/any_other_name/

$CLICKHOUSE_CLIENT --allow_drop_detached=1 --query="ALTER TABLE drop_detached DROP DETACHED PART '../1_2_2_0'" 2>&1 | grep "Invalid part name" > /dev/null && echo 'OK'
$CLICKHOUSE_CLIENT --allow_drop_detached=1 --query="ALTER TABLE drop_detached DROP DETACHED PART '0_1_1_0'"
$CLICKHOUSE_CLIENT --allow_drop_detached=1 --query="ALTER TABLE drop_detached DROP DETACHED PART 'attaching_0_6_6_0'"
$CLICKHOUSE_CLIENT --allow_drop_detached=1 --query="ALTER TABLE drop_detached DROP DETACHED PART 'delete_tmp_0_7_7_0'"
$CLICKHOUSE_CLIENT --allow_drop_detached=1 --query="ALTER TABLE drop_detached DROP DETACHED PART 'any_other_name'"

$CLICKHOUSE_CLIENT --query="SElECT directory_name FROM system.detached_parts WHERE table='drop_detached' AND database='${cur_db}' FORMAT TSV";
$CLICKHOUSE_CLIENT --query="DROP TABLE drop_detached";
$CLICKHOUSE_CLIENT --query="SYSTEM START MERGES";
