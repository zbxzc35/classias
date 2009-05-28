/*
 *		Training Maximum Entropy models with L-BFGS.
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
 *     * Neither the name of the Northwestern University, University of Tokyo,
 *       nor the names of its contributors may be used to endorse or promote
 *       products derived from this software without specific prior written
 *       permission.
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

#ifndef __CLASSIAS_MAXENT_H__
#define __CLASSIAS_MAXENT_H__

#include <float.h>
#include <cmath>
#include <ctime>
#include <iostream>

#include "lbfgs.h"
#include "evaluation.h"
#include "parameters.h"

namespace classias
{

template <
    class key_tmpl,
    class label_tmpl,
    class value_tmpl,
    class model_tmpl,
    class traits_tmpl
>
class linear_multi_classifier
{
public:
    typedef key_tmpl key_type;
    typedef key_tmpl label_type;
    typedef value_tmpl value_type;
    typedef model_tmpl model_type;
    typedef traits_tmpl traits_type;

protected:
    typedef std::vector<value_type> scores_type;
    typedef std::vector<label_type> labels_type;

    model_type& m_model;
    scores_type m_scores;
    scores_type m_probs;
    labels_type m_labels;
    traits_type& m_traits;

    int         m_argmax;
    value_type  m_norm;

public:
    linear_multi_classifier(model_type& model, traits_type& traits)
        : m_model(model), m_traits(traits)
    {
        clear();
    }

    virtual ~linear_multi_classifier()
    {
    }

    inline void clear()
    {
        m_norm = 0.;
        for (int i = 0;i < this->size();++i) {
            m_scores[i] = 0.;
            m_probs[i] = 0.;
        }
    }

    inline void resize(int n)
    {
        m_scores.resize(n);
        m_probs.resize(n);
        m_labels.resize(n);
    }

    inline int size() const
    {
        return (int)m_scores.size();
    }

    inline int argmax() const
    {
        return m_argmax;
    }

    inline value_type score(int i)
    {
        return m_scores[i];
    }

    inline value_type prob(int i)
    {
        return m_probs[i];
    }

    inline const label_type& label(int i)
    {
        return m_labels[i];
    }

    inline void operator()(int i, const key_type& key, const label_type& label, const value_type& value)
    {
        int fid = m_traits.forward(key, label);
        if (0 <= fid) {
            m_scores[i] += m_model[fid] * value;
        }
    }

    template <class iterator_type>
    inline void accumulate(int i, iterator_type first, iterator_type last, const label_type& label)
    {
        m_scores[i] = 0.;
        m_labels[i] = label;
        for (iterator_type it = first;it != last;++it) {
            this->operator()(i, it->first, label, it->second);
        }
    }

    template <class iterator_type>
    inline void add_to(value_type* v, iterator_type first, iterator_type last, const label_type& label, value_type value)
    {
        for (iterator_type it = first;it != last;++it) {
            int fid = m_traits.forward(it->first, label);
            if (0 <= fid) {
                v[fid] += value * it->second;
            }
        }        
    }

    inline bool finalize(bool prob)
    {
        if (m_scores.size() == 0) {
            return false;
        }

        // Find the argmax index.
        m_argmax = 0;
        double vmax = m_scores[0];
        for (int i = 0;i < this->size();++i) {
            if (vmax < m_scores[i]) {
                m_argmax = i;
                vmax = m_scores[i];
            }
        }

        if (prob) {
            // Compute the exponents of scores.
            for (int i = 0;i < this->size();++i) {
                m_probs[i] = std::exp(m_scores[i]);
            }

            // Compute the partition factor, starting from the maximum value.
            m_norm = m_probs[m_argmax];
            for (int i = 0;i < this->size();++i) {
                if (i != m_argmax) {
                    m_norm += m_probs[i];
                }
            }

            // Normalize the probabilities.
            for (int i = 0;i < this->size();++i) {
                m_probs[i] /= m_norm;
            }
        }

        return true;
    }
};




/**
 * Training a log-linear model using the maximum entropy modeling.
 *  @param  data_tmpl           Training data class.
 *  @param  value_tmpl          The type for computation.
 */
template <
    class data_tmpl,
    class value_tmpl = double
>
class trainer_maxent : public lbfgs_solver
{
protected:
    /// A type representing a data set for training.
    typedef data_tmpl data_type;
    /// A type representing values for internal computations.
    typedef value_tmpl value_type;
    /// A synonym of this class.
    typedef trainer_maxent<data_type, value_type> this_class;
    /// A type representing an instance in the training data.
    typedef typename data_type::instance_type instance_type;
    typedef typename data_type::traits_type traits_type;
    /// A type representing a candidate for an instance.
    typedef typename instance_type::candidate_type candidate_type;
    typedef typename instance_type::feature_type feature_type;
    /// A type representing a label.
    typedef typename instance_type::label_type label_type;
    /// A type providing a read-only random-access iterator for instances.
    typedef typename data_type::const_iterator const_iterator;

    typedef linear_multi_classifier<feature_type, label_type, value_type, value_type const*, traits_type> classifier_type;


    /// An array [K] of observation expectations.
    value_type *m_oexps;
    /// An array [K] of model expectations.
    value_type *m_mexps;
    /// An array [K] of feature weights.
    value_type *m_weights;
    /// An array [M] of scores for candidate labels.
    value_type *m_scores;

    label_type m_num_labels;

    /// A data set for training.
    const data_type* m_data;
    /// A group number used for holdout evaluation.
    int m_holdout;

    /// Parameters interface.
    parameter_exchange m_params;
    /// Regularization type.
    std::string m_regularization;
    /// Regularization sigma;
    value_type m_regularization_sigma;
    /// The start index of regularization.
    int m_regularization_start;
    /// The number of memories in L-BFGS.
    int m_lbfgs_num_memories;
    /// L-BFGS epsilon for convergence.
    value_type m_lbfgs_epsilon;
    /// Number of iterations for stopping criterion.
    int m_lbfgs_stop;
    /// The delta threshold for stopping criterion.
    value_type m_lbfgs_delta;
    /// Maximum number of L-BFGS iterations.
    int m_lbfgs_maxiter;
    /// Line search algorithm.
    std::string m_lbfgs_linesearch;
    /// The maximum number of trials for the line search algorithm.
    int m_lbfgs_max_linesearch;

    /// L1-regularization constant.
    value_type m_c1;
    /// L2-regularization constant.
    value_type m_c2;

    /// An output stream to which this object outputs log messages.
    std::ostream* m_os;
    /// An internal variable (previous timestamp).
    clock_t m_clk_prev;

public:
    trainer_maxent()
    {
        m_oexps = 0;
        m_mexps = 0;
        m_weights = 0;
        m_scores = 0;
        m_num_labels = 0;
        m_regularization_start = 0;

        clear();
    }

    virtual ~trainer_maxent()
    {
        clear();
    }

    void clear()
    {
        delete[] m_weights;
        delete[] m_mexps;
        delete[] m_oexps;
        delete[] m_scores;
        m_oexps = 0;
        m_mexps = 0;
        m_weights = 0;
        m_scores = 0;

        m_data = NULL;
        m_os = NULL;
        m_holdout = -1;

        // Initialize the parameters.
        m_params.init("regularization", &m_regularization, "L2",
            "Regularization method (prior):\n"
            "{'': no regularization, 'L1': L1-regularization, 'L2': L2-regularization}");
        m_params.init("regularization.sigma", &m_regularization_sigma, 5.0,
            "Regularization coefficient (sigma).");
        m_params.init("lbfgs.num_memories", &m_lbfgs_num_memories, 6,
            "The number of corrections to approximate the inverse hessian matrix.");
        m_params.init("lbfgs.epsilon", &m_lbfgs_epsilon, 1e-5,
            "Epsilon for testing the convergence of the log likelihood.");
        m_params.init("lbfgs.stop", &m_lbfgs_stop, 10,
            "The duration of iterations to test the stopping criterion.");
        m_params.init("lbfgs.delta", &m_lbfgs_delta, 1e-5,
            "The threshold for the stopping criterion; an L-BFGS iteration stops when the\n"
            "improvement of the log likelihood over the last ${lbfgs.stop} iterations is\n"
            "no greater than this threshold.");
        m_params.init("lbfgs.max_iterations", &m_lbfgs_maxiter, INT_MAX,
            "The maximum number of L-BFGS iterations.");
        m_params.init("lbfgs.linesearch", &m_lbfgs_linesearch, "MoreThuente",
            "The line search algorithm used in L-BFGS updates:\n"
            "{'MoreThuente': More and Thuente's method, 'Backtracking': backtracking}");
        m_params.init("lbfgs.max_linesearch", &m_lbfgs_max_linesearch, 20,
            "The maximum number of trials for the line search algorithm.");
    }

    parameter_exchange& params()
    {
        return m_params;
    }

    const value_type* get_weights() const
    {
        return m_weights;
    }

    virtual value_type lbfgs_evaluate(
        const value_type *x,
        value_type *g,
        const int n,
        const value_type step
        )
    {
        int i;
        value_type loss = 0, norm = 0;
        const data_type& data = *m_data;
        classifier_type cls(x, const_cast<traits_type&>(data.traits));

        // Initialize the model expectations as zero.
        for (i = 0;i < n;++i) {
            m_mexps[i] = 0.;
        }
        cls.resize(m_num_labels);

        // For each instance in the data.
        for (const_iterator iti = data.begin();iti != data.end();++iti) {
            int itrue = -1;
            typename instance_type::const_iterator itc;

            // Exclude instances for holdout evaluation.
            if (iti->get_group() == m_holdout) {
                continue;
            }

            // Compute score[i] for each candidate #i.
            for (i = 0, itc = iti->begin();itc != iti->end();++i, ++itc) {
                cls.accumulate(i, itc->begin(), itc->end(), itc->get_label());
                if (itc->get_truth()) {
                    itrue = i;
                }
            }

            cls.finalize(true);

            // Accumulate the model expectations of features.
            for (i = 0, itc = iti->begin();itc != iti->end();++i, ++itc) {
                cls.add_to(m_mexps, itc->begin(), itc->end(), itc->get_label(), i);
            }

            // Accumulate the loss for predicting the instance.
            loss -= std::log(cls.prob(itrue));
        }

        // Compute the gradients.
        for (int i = 0;i < n;++i) {
            g[i] = -(m_oexps[i] - m_mexps[i]);
        }

        // Apply L2 regularization if necessary.
        if (m_c2 != 0.) {
            value_type norm = 0.;
            for (int i = m_regularization_start;i < n;++i) {
                g[i] += (m_c2 * x[i]);
                norm += x[i] * x[i];
            }
            loss += (m_c2 * norm * 0.5);
        }

        return loss;
    }

    virtual int lbfgs_progress(
        const value_type *x,
        const value_type *g,
        const value_type fx,
        const value_type xnorm,
        const value_type gnorm,
        const value_type step,
        int n,
        int k,
        int ls)
    {
        // Compute the duration required for this iteration.
        std::ostream& os = *m_os;
        clock_t duration, clk = std::clock();
        duration = clk - m_clk_prev;
        m_clk_prev = clk;

        // Count the number of active features.
        int num_active = 0;
        for (int i = 0;i < n;++i) {
            if (x[i] != 0.) {
                ++num_active;
            }
        }

        // Output the current progress.
        os << "***** Iteration #" << k << " *****" << std::endl;
        os << "Log-likelihood: " << -fx << std::endl;
        os << "Feature norm: " << xnorm << std::endl;
        os << "Error norm: " << gnorm << std::endl;
        os << "Active features: " << num_active << " / " << n << std::endl;
        os << "Line search trials: " << ls << std::endl;
        os << "Line search step: " << step << std::endl;
        os << "Seconds required for this iteration: " <<
            duration / (double)CLOCKS_PER_SEC << std::endl;
        os.flush();

        // Holdout evaluation if necessary.
        if (m_holdout != -1) {
            holdout_evaluation();
        }

        // Output an empty line.
        os << std::endl;
        os.flush();

        return 0;
    }

    int train(
        const data_type& data,
        std::ostream& os,
        int holdout = -1,
        bool false_analysis = false
        )
    {
        const size_t K = data.traits.num_features();

        // Initialize feature expectations and weights.
        m_oexps = new double[K];
        m_mexps = new double[K];
        m_weights = new double[K];
        for (size_t k = 0;k < K;++k) {
            m_oexps[k] = 0.;
            m_mexps[k] = 0.;
            m_weights[k] = 0.;
        }
        m_holdout = holdout;

        // Set the internal parameters.
        if (m_regularization == "L1" || m_regularization == "l1") {
            m_c1 = 1.0 / m_regularization_sigma;
            m_c2 = 0.;
            m_lbfgs_linesearch = "Backtracking";
        } else if (m_regularization == "L2" || m_regularization == "l2") {
            m_c1 = 0.;
            m_c2 = 1.0 / (m_regularization_sigma * m_regularization_sigma);
        } else {
            m_c1 = 0.;
            m_c2 = 0.;
        }

        m_regularization_start = data.get_user_feature_start();

        // Report the training parameters.
        os << "Training a maximum entropy model" << std::endl;
        m_params.show(os);
        os << std::endl;

        // Compute observation expectations of the features.
        m_num_labels = 0;
        for (const_iterator iti = data.begin();iti != data.end();++iti) {
            // Skip instances for holdout evaluation.
            if (iti->get_group() == m_holdout) {
                continue;
            }

            // Compute the observation expectations.
            typename instance_type::const_iterator itc;
            for (itc = iti->begin();itc != iti->end();++itc) {
                if (itc->get_truth()) {
                    // m_oexps[k] += 1.0 * (*itc)[k].
                    itc->add_to(m_oexps, 1.0);
                }
            }

            if (m_num_labels < (label_type)iti->size()) {
                m_num_labels = (label_type)iti->size();
            }
        }

        // Initialze the variables used by callback functions.
        m_os = &os;
        m_data = &data;
        m_clk_prev = clock();
        m_scores = new double[m_num_labels];

        // Call the L-BFGS solver.
        int ret = lbfgs_solve(
            (const int)K,
            m_weights,
            NULL,
            m_lbfgs_num_memories,
            m_lbfgs_epsilon,
            m_lbfgs_stop,
            m_lbfgs_delta,
            m_lbfgs_maxiter,
            m_lbfgs_linesearch,
            m_lbfgs_max_linesearch,
            m_c1,
            m_regularization_start
            );

        // Report the result from the L-BFGS solver.
        lbfgs_output_status(os, ret);

        return ret;
    }

    void holdout_evaluation()
    {
        std::ostream& os = *m_os;
        const data_type& data = *m_data;
        accuracy acc;
        confusion_matrix matrix(data.labels.size());
        const value_type *x = m_weights;
        classifier_type cls(x, const_cast<traits_type&>(data.traits));

        // Loop over instances.
        for (const_iterator iti = data.begin();iti != data.end();++iti) {
            // Exclude instances for holdout evaluation.
            if (iti->get_group() != m_holdout) {
                continue;
            }

            int i;
            int idx_true = -1;
            typename instance_type::const_iterator itc;
            for (i = 0, itc = iti->begin();itc != iti->end();++i, ++itc) {
                cls.accumulate(i, itc->begin(), itc->end(), itc->get_label());
                if (itc->get_truth()) {
                    idx_true = i;
                }
            }

            cls.finalize(false);

            int idx_max = cls.argmax();

            acc.set(idx_true == idx_max);
            if (idx_true != -1) {
                matrix(cls.label(idx_true), cls.label(idx_max))++;
            }
        }

        // Report accuracy, precision, recall, and f1 score.
        acc.output(os);
        matrix.output_micro(os, data.positive_labels.begin(), data.positive_labels.end());
    }
};

};

#endif/*__CLASSIAS_MAXENT_H__*/
