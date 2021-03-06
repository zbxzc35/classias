/*
 *		Data I/O for multi-class classification.
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
#include <classias/classify/linear/multi.h>
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
<instance>      ::= <class> ("\t" <attribute>)+ <br>
<class>         ::= <string>
<attribute>     ::= <name> [ ":" <weight> ]
<name>          ::= <string>
<weight>        ::= <numeric>
<br>            ::= "\n"
*/

template <
    class instance_type,
    class attributes_quark_type,
    class label_quark_type
>
static void
read_line(
    const std::string& line,
    instance_type& instance,
    attributes_quark_type& attributes,
    label_quark_type& labels,
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

    // Set the instance label and weight.
    instance.set_label(labels(name));
    instance.set_weight(value);

    // Set attributes for the instance.
    for (++itv;itv != values.end();++itv) {
        if (!itv->empty()) {
            double value;
            std::string name;
            get_name_value(*itv, name, value, opt.value_separator);
            if (opt.filter_string.empty() || REGEX_SEARCH(name, opt.filter)) {
                instance.append(attributes(name), value);
            }
        }
    }

    // Include a bias feature if necessary.
    if (opt.bias != 0.) {
        instance.append(attributes("__BIAS__"), opt.bias);
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
        int aid = (int)data.attributes("__BIAS__");
        if (aid != 0) {
            throw invalid_data("A bias attribute could not obtain #0");
        }
        // We will reserve the bias feature(s) in finalize_data() function.
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

        read_line(line, inst, data.attributes, data.labels, opt, lines);
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
    typedef int int_t;

    // If necessary, reserve early feature numbers for bias features.
    if (opt.bias != 0.) {
        int_t aid = (int_t)data.attributes("__BIAS__");
        if (aid != 0) {
            throw invalid_data("A bias attribute could not obtain #0");
        }
        data.generate_bias_features(aid);
    }

    // Generate features that associate attributes and labels.
    data.generate_features();

    // Set positive labels.
    for (int_t l = 0;l < data.num_labels();++l) {
        if (opt.negative_labels.find(data.labels.to_item(l)) == opt.negative_labels.end()) {
            data.append_positive_label(l);
        }
    }
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
    typedef int int_t;
    typedef typename data_type::attributes_quark_type attributes_quark_type;
    typedef typename attributes_quark_type::item_type attribute_type;
    typedef typename data_type::labels_quark_type labels_quark_type;
    typedef typename labels_quark_type::item_type label_type;
    typedef typename model_type::value_type value_type;

    // Open a model file for writing.
    std::ofstream os(opt.model.c_str());

    // Output a model type.
    os << "@classias\tlinear\tmulti\t";
    os << data.feature_generator.name() << std::endl;

    // Output a set of labels.
    for (int_t l = 0;l < data.num_labels();++l) {
        os << "@label\t" << data.labels.to_item(l) << std::endl;
    }

    // Store the feature weights.
    for (int_t i = 0;i < data.num_features();++i) {
        value_type w = model[i];
        if (w != 0.) {
            int_t a, l;
            data.feature_generator.backward(i, a, l);
            const std::string& attr = data.attributes.to_item(a);
            const std::string& label = data.labels.to_item(l);
            if (attr == "__BIAS__") {
                w *= opt.bias;
            }
            os << w << '\t' << attr << '\t' << label << std::endl;
        }
    }
}

int multi_train(option& opt)
{
    // Branches for training algorithms.
    if (opt.algorithm == "lbfgs.logistic") {
        if (opt.type == option::TYPE_MULTI_SPARSE) {
            return train<
                classias::nsdata,
                classias::train::lbfgs_logistic_multi<classias::nsdata>
            >(opt);
        } else if (opt.type == option::TYPE_MULTI_DENSE) {
            return train<
                classias::msdata,
                classias::train::lbfgs_logistic_multi<classias::msdata>
            >(opt);
        }
    } else if (opt.algorithm == "averaged_perceptron") {
        if (opt.type == option::TYPE_MULTI_SPARSE) {
            return train<
                classias::nsdata,
                classias::train::online_scheduler_multi<
                    classias::nsdata,
                    classias::train::averaged_perceptron_multi<
                        classias::classify::linear_multi<classias::weight_vector>
                        >
                    >
                >(opt);
        } else if (opt.type == option::TYPE_MULTI_DENSE) {
            return train<
                classias::msdata,
                classias::train::online_scheduler_multi<
                    classias::msdata,
                    classias::train::averaged_perceptron_multi<
                        classias::classify::linear_multi<classias::weight_vector>
                        >
                    >
                >(opt);
        }
    } else if (opt.algorithm == "pegasos.logistic") {
        if (opt.type == option::TYPE_MULTI_SPARSE) {
            return train<
                classias::nsdata,
                classias::train::online_scheduler_multi<
                    classias::nsdata,
                    classias::train::pegasos_multi<
                        classias::classify::linear_multi_logistic<classias::weight_vector>
                        >
                    >
                >(opt);
        } else if (opt.type == option::TYPE_MULTI_DENSE) {
            return train<
                classias::msdata,
                classias::train::online_scheduler_multi<
                    classias::msdata,
                    classias::train::pegasos_multi<
                        classias::classify::linear_multi_logistic<classias::weight_vector>
                        >
                    >
                >(opt);
        }
    } else if (opt.algorithm == "truncated_gradient.logistic") {
        if (opt.type == option::TYPE_MULTI_SPARSE) {
            return train<
                classias::nsdata,
                classias::train::online_scheduler_multi<
                    classias::nsdata,
                    classias::train::truncated_gradient_multi<
                        classias::classify::linear_multi_logistic<classias::weight_vector>
                        >
                    >
                >(opt);
        } else if (opt.type == option::TYPE_MULTI_DENSE) {
            return train<
                classias::msdata,
                classias::train::online_scheduler_multi<
                    classias::msdata,
                    classias::train::truncated_gradient_multi<
                        classias::classify::linear_multi_logistic<classias::weight_vector>
                        >
                    >
                >(opt);
        }
    }
    throw invalid_algorithm(opt.algorithm);
}
