#include <Processors/ResizeProcessor.h>


namespace DB
{

ResizeProcessor::Status ResizeProcessor::prepare()
{
    bool is_first_output = true;
    auto output_end = current_output;

    bool all_outs_full_or_unneeded = true;
    bool all_outs_finished = true;

    bool is_first_input = true;
    auto input_end = current_input;

    bool all_inputs_finished = true;

    auto is_end_input = [&]() { return !is_first_input && current_input == input_end; };
    auto is_end_output = [&]() { return !is_first_output && current_output == output_end; };

    auto inc_current_input = [&]()
    {
        is_first_input = false;
        ++current_input;

        if (current_input == inputs.end())
            current_input = inputs.begin();
    };

    auto inc_current_output = [&]()
    {
        is_first_output = false;
        ++current_output;

        if (current_output == outputs.end())
            current_output = outputs.begin();
    };

    /// Find next output where can push.
    auto get_next_out = [&, this]() -> OutputPorts::iterator
    {
        while (!is_end_output())
        {
            if (!current_output->isFinished())
            {
                all_outs_finished = false;

                if (current_output->canPush())
                {
                    all_outs_full_or_unneeded = false;
                    auto res_output = current_output;
                    inc_current_output();
                    return res_output;
                }
            }

            inc_current_output();
        }

        return outputs.end();
    };

    /// Find next input from where can pull.
    auto get_next_input = [&, this]() -> InputPorts::iterator
    {
        while (!is_end_input())
        {
            if (!current_input->isFinished())
            {
                all_inputs_finished = false;

                current_input->setNeeded();
                if (current_input->hasData())
                {
                    auto res_input = current_input;
                    inc_current_input();
                    return res_input;
                }
            }

            inc_current_input();
        }

        return inputs.end();
    };

    auto get_status_if_no_outputs = [&]() -> Status
    {
        if (all_outs_finished)
        {
            for (auto & in : inputs)
                in.close();

            return Status::Finished;
        }

        if (all_outs_full_or_unneeded)
        {
            for (auto & in : inputs)
                in.setNotNeeded();

            return Status::PortFull;
        }

        /// Now, we pushed to output, and it must be full.
        return Status::PortFull;
    };

    auto get_status_if_no_inputs = [&]() -> Status
    {
        if (all_inputs_finished)
        {
            for (auto & out : outputs)
                out.finish();

            return Status::Finished;
        }

        return Status::NeedData;
    };

    /// Set all inputs needed in order to evenly process them.
    /// Otherwise, in case num_outputs < num_inputs and chunks are consumed faster than produced,
    ///   some inputs can be skipped.
//    auto set_all_unprocessed_inputs_needed = [&]()
//    {
//        for (; cur_input != inputs.end(); ++cur_input)
//            if (!cur_input->isFinished())
//                cur_input->setNeeded();
//    };

    while (!is_end_input() && !is_end_output())
    {
        auto output = get_next_out();
        auto input = get_next_input();

        if (output == outputs.end())
            return get_status_if_no_outputs();


        if (input == inputs.end())
            return get_status_if_no_inputs();

        output->push(input->pull());
    }

    if (is_end_input())
        return get_status_if_no_outputs();

    /// cur_input == inputs_end()
    return get_status_if_no_inputs();
}

IProcessor::Status ResizeProcessor::prepare(const InputRawPtrs & updated_inputs, const OutputRawPtrs & updated_outputs)
{
    if (!initialized)
    {
        initialized = true;

        for (auto & input : inputs)
        {
            input_ports_status[&input] = InputStatus::NotNeeded;
            not_needed_inputs.emplace(&input);
        }

        for (auto & output : outputs)
            output_ports_status[&output] = OutputStatus::NotActive;
    }

    for (auto & output : updated_outputs)
    {
        if (output->isFinished())
        {
            if (output_ports_status[output] != OutputStatus::Finished)
            {
                ++num_finished_outputs;
                output_ports_status[output] = OutputStatus::Finished;
            }

            continue;
        }

        if (output->canPush())
        {
            if (output_ports_status[output] != OutputStatus::NeedData)
            {
                output_ports_status[output] = OutputStatus::NeedData;
                waiting_outputs.push(output);
            }
        }
    }

    if (num_finished_outputs == outputs.size())
    {
        for (auto & input : inputs)
            input.close();

        return Status::Finished;
    }

    for (auto & input : updated_inputs)
    {
        if (input->isFinished())
        {
            if (input_ports_status[input] != InputStatus::Finished)
            {
                input_ports_status[input] = InputStatus::Finished;
                ++num_finished_inputs;
            }
            continue;
        }

        if (input->hasData())
        {
            if (input_ports_status[input] != InputStatus::HasData)
            {
                input_ports_status[input] = InputStatus::HasData;
                inputs_with_data.push(input);
            }
        }
        else
        {
            input->setNotNeeded();

            if (input_ports_status[input] != InputStatus::NotNeeded)
            {
                input_ports_status[input] = InputStatus::NotNeeded;
                not_needed_inputs.push(input);
            }
        }
    }

    while (!waiting_outputs.empty() && !inputs_with_data.empty())
    {
        auto waiting_output = waiting_outputs.front();
        waiting_outputs.pop();

        auto input_with_data = inputs_with_data.front();
        inputs_with_data.pop();

        waiting_output->pushData(input_with_data->pullData());
        input_ports_status[input_with_data] = InputStatus::NotNeeded;
        output_ports_status[waiting_output] = OutputStatus::NotActive;

        if (input_with_data->isFinished())
        {
            if (input_ports_status[input_with_data] != InputStatus::Finished)
            {
                input_ports_status[input_with_data] = InputStatus::Finished;
                ++num_finished_inputs;
            }
        }
    }

    if (num_finished_inputs == inputs.size())
    {
        for (auto & output : outputs)
            output.finish();

        return Status::Finished;
    }

    size_t num_needed_inputs = waiting_outputs.size();
    while (!not_needed_inputs.empty() && num_needed_inputs > 0)
    {
        auto input = not_needed_inputs.front();
        not_needed_inputs.pop();

        input->setNeeded();
        input_ports_status[input] = InputStatus::Needed;
        --num_needed_inputs;
    }

    if (!waiting_outputs.empty())
        return Status::NeedData;

    return Status::PortFull;
}

}

