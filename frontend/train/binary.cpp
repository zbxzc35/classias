/*
 *		Data I/O for binary-class classification.
 *
 * Copyright (c) 2008,2009 Naoaki Okazaki
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the names of the authors nor the names of its contributors
 *       may be used to endorse or promote products derived from this
 *       software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* $Id$ */

#ifdef  HAVE_CONFIG_H
#include <config.h>
#endif/*HAVE_CONFIG_H*/

#include <iostream>
#include <string>

#include <classias/classias.h>
#include <classias/classify/linear/binary.h>
#include <classias/train/lbfgs.h>
#include <classias/train/averaged_perceptron.h>
#include <classias/train/pegasos.h>
#include <classias/train/truncated_gradient.h>
#include <classias/train/online_scheduler.h>

#include "option.h"
#include "tokenize.h"
#include "train.h"

/*
<line>          ::= <comment> | <instance> | <br>
<comment>       ::= "#" <string> <br>
<instance>      ::= <class> ("\t" <feature>)+ <br>
<class>         ::= ("-1" | "1" | "+1") [ ":" <weight> ]
<feature>       ::= <name> [ ":" <weight> ]
<name>          ::= <string>
<weight>        ::= <numeric>
<br>            ::= "\n"
*/

template <
    class instance_type,
    class features_quark_type
>
static void
read_line(
    const std::string& line,
    instance_type& instance,
    features_quark_type& features,
    const option& opt,
    int lines = 0
    )
{
    double value;
    std::string name;

    // Split the line with tab characters.
    tokenizer values(line, opt.token_separator);
    tokenizer::iterator itv = values.begin();
    if (itv == values.end()) {
        throw invalid_data("no field found in the line", line, lines);
    }

    // Make sure that the first token (class) is not empty.
    if (itv->empty()) {
        throw invalid_data("an empty label found", line, lines);
    }

    // Parse the instance label.
    get_name_value(*itv, name, value, opt.value_separator);

    // Set the class label of this instance.
    if (name == "+1" || name == "1") {
        instance.set_label(true);
    } else if (name == "-1") {
        instance.set_label(false);
    } else {
        throw invalid_data("a class label must be either '+1', '1', or '-1'", line, lines);
    }

    // Set the instance weight.
    instance.set_weight(value);

    // Set featuress for the instance.
    for (++itv;itv != values.end();++itv) {
        if (!itv->empty()) {
            get_name_value(*itv, name, value, opt.value_separator);
            if (opt.filter_string.empty() || REGEX_SEARCH(name, opt.filter)) {
                instance.append(features(name), value);
            }
        }
    }

    // Include a bias feature if necessary.
    if (opt.bias != 0.) {
        instance.append(features("__BIAS__"), opt.bias);
    }
}

template <
    class data_type
>
static void
read_stream(
    std::istream& is,
    data_type& data,
    const option& opt,
    int group = 0
    )
{
    int lines = 0;
    typedef typename data_type::instance_type instance_type;

    // If necessary, generate a bias attribute here to reserve feature #0.
    if (opt.bias != 0.) {
        int fid = (int)data.attributes("__BIAS__");
        if (fid != 0) {
            throw invalid_data("A bias attribute could not obtain #0");
        }
        data.set_user_feature_start(fid+1);
    }

    for (;;) {
        // Read a line.
        std::string line;
        std::getline(is, line);
        if (is.eof()) {
            break;
        }
        ++lines;

        // Skip an empty line.
        if (line.empty()) {
            continue;
        }

        // Skip a comment line.
        if (line.compare(0, 1, "#") == 0) {
            continue;
        }

        // Create a new instance.
        instance_type& inst = data.new_element();
        inst.set_group(group);

        // Read the instance.
        read_line(line, inst, data.attributes, opt, lines);
    }
}

template <
    class data_type
>
static void
finalize_data(
    data_type& data,
    const option& opt
    )
{
    // Nothing to do.
}

template <
    class data_type,
    class model_type
>
static void
output_model(
    data_type& data,
    const model_type& model,
    const option& opt
    )
{
    typedef typename data_type::attributes_quark_type attributes_quark_type;
    typedef typename attributes_quark_type::value_type aid_type;
    const attributes_quark_type& attributes = data.attributes;
    typedef typename model_type::value_type value_type;

    // Open a model file for writing.
    std::ofstream os(opt.model.c_str());

    // Output a model type.
    os << "@classias\tlinear\tbinary" << std::endl;

    // Store the feature weights.
    for (aid_type i = 0;i < attributes.size();++i) {
        value_type w = model[i];
        if (w != 0.) {
            const std::string& attr = attributes.to_item(i);
            if (attr == "__BIAS__") {
                w *= opt.bias;
            }
            os << w << '\t' << attr << std::endl;
        }
    }
}

int binary_train(option& opt)
{
    // Branches for training algorithms.
    if (opt.algorithm == "lbfgs.logistic") {
        return train<
            classias::bsdata,
            classias::train::lbfgs_logistic_binary<classias::bsdata>
        >(opt);
    } else if (opt.algorithm == "averaged_perceptron") {
        return train<
            classias::bsdata,
            classias::train::online_scheduler_binary<
                classias::bsdata,
                classias::train::averaged_perceptron_binary<
                    classias::classify::linear_binary<classias::weight_vector>
                    >
                >
            >(opt);
    } else if (opt.algorithm == "pegasos.logistic") {
        return train<
            classias::bsdata,
            classias::train::online_scheduler_binary<
                classias::bsdata,
                classias::train::pegasos_binary<
                    classias::classify::linear_binary_logistic<classias::weight_vector>
                    >
                >
            >(opt);
    } else if (opt.algorithm == "pegasos.hinge") {
        return train<
            classias::bsdata,
            classias::train::online_scheduler_binary<
                classias::bsdata,
                classias::train::pegasos_binary<
                    classias::classify::linear_binary_hinge<classias::weight_vector>
                    >
                >
            >(opt);
    } else if (opt.algorithm == "truncated_gradient.logistic") {
        return train<
            classias::bsdata,
            classias::train::online_scheduler_binary<
                classias::bsdata,
                classias::train::truncated_gradient_binary<
                    classias::classify::linear_binary_logistic<classias::weight_vector>
                    >
                >
            >(opt);
    } else if (opt.algorithm == "truncated_gradient.hinge") {
        return train<
            classias::bsdata,
            classias::train::online_scheduler_binary<
                classias::bsdata,
                classias::train::truncated_gradient_binary<
                    classias::classify::linear_binary_hinge<classias::weight_vector>
                    >
                >
            >(opt);
    } else {
        throw invalid_algorithm(opt.algorithm);
    }
}
