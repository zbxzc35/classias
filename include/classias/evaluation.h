/*
 *		Utilities for evaluation.
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

#ifndef __CLASSIAS_EVALUATION_H__
#define __CLASSIAS_EVALUATION_H__

#include <iomanip>
#include <vector>

namespace classias
{

/**
 * Accuracy counter.
 *
 *  This class counts the number of true instances and the total number of
 *  instances. Call set() with a boolean argument indicating whether the
 *  classification for an instance is successful (\c true) or not (\c false).
 */
class accuracy
{
protected:
    int m_m;    ///< The number of matches.
    int m_n;    ///< The total number of instances.

public:
    /**
     * Constructs an object.
     */
    accuracy() : m_m(0), m_n(0)
    {
    }

    /**
     * Destructs the object.
     */
    virtual ~accuracy()
    {
    }

    /**
     * Increments the number of correct/incorrect instances.
     *  @param  b           A truth of an instance.
     */
    inline void set(bool b)
    {
        m_m += static_cast<int>(b);
        ++m_n;
    }

    /**
     * Gets the accuracy.
     *  @return double      The accuracy.
     */
    inline operator double() const
    {
        return (0 < m_n ? m_m / (double)m_n : 0.);
    }

    /**
     * Outputs the accuracy score.
     *  @param  os          The output stream.
     */
    void output(std::ostream& os) const
    {
        double acc = (0 < m_n ? m_m / (double)m_n : 0);
        os << "Accuracy: " <<
            std::fixed << std::setprecision(4) << static_cast<double>(*this) <<
            std::setprecision(6) << 
            " (" << m_m << "/" << m_n << ")" << std::endl;
        os.unsetf(std::ios::fixed);
    }
};



/**
 * Counter for precision, recall, and F1 scores.
 */
class precall
{
protected:
    /// A counter for each label.
    struct label_stat
    {
        int num_match;          ///< The number of true positives.
        int num_reference;      ///< The number of reference positives.
        int num_prediction;     ///< The number of predicted positives.

        label_stat() :
            num_match(0), num_reference(0), num_prediction(0)
        {
        }
    };

    std::vector<label_stat> m_stat; ///< The label-wise stats.

public:
    /**
     * Constructs an object.
     *  @param  N           The number of labels.
     */
    precall(int N)
    {
        resize(N);
    }

    /**
     * Destructs an object.
     */
    virtual ~precall()
    {
    }

    /**
     * Resize the number of labels.
     *  @param  N           The number of labels.
     */
    void resize(int N)
    {
        m_stat.resize(N);
    }

    /**
     * Sets a pair of predicted and reference labels.
     *  @param  p           The predicted label.
     *  @param  r           The reference label.
     */
    void set(int p, int r)
    {
        m_stat[r].num_reference++;
        m_stat[p].num_prediction++;
        if (r == p) m_stat[p].num_match++;
    }

    template <class labels_type, class positive_iterator_type>
    void output_labelwise(
        std::ostream& os,
        const labels_type& labels,
        positive_iterator_type pb,
        positive_iterator_type pe
        ) const
    {
        os << "Performance by label (#match, #model, #ref) (precision, recall, F1):" << std::endl;
        for (positive_iterator_type it = pb;it != pe;++it) {
            int num_match = m_stat[*it].num_match;
            int num_prediction = m_stat[*it].num_prediction;
            int num_reference = m_stat[*it].num_reference;
            double precision = divide(num_match, num_prediction);
            double recall = divide(num_match, num_reference);
            double f1score = divide(2 * precision * recall, precision + recall);

            os <<
                "    " << labels.to_item(*it) << ": " <<
                "(" << num_match << ", " << num_prediction << ", " << num_reference << ") " <<
                "(" << precision << ", " << recall << ", " << f1score << ")" << std::endl;
        }
    }

    /**
     * Outputs micro-average precision, recall, F1 scores.
     *  @param  os          The output stream.
     *  @param  pb          The iterator for the first element of the
     *                      positive labels.
     *  @param  pe          The iterator just beyond the last element
     *                      of the positive labels.
     */
    template <class positive_iterator_type>
    void output_micro(
        std::ostream& os,
        positive_iterator_type pb,
        positive_iterator_type pe
        ) const
    {
        int num_match = 0;
        int num_reference = 0;
        int num_prediction = 0;

        for (positive_iterator_type it = pb;it != pe;++it) {
            num_match += m_stat[*it].num_match;
            num_reference += m_stat[*it].num_reference;
            num_prediction += m_stat[*it].num_prediction;
        }

        double precision = divide(num_match, num_prediction);
        double recall = divide(num_match, num_reference);
        double f1score = divide(2 * precision * recall, precision + recall);

        os << "Micro P, R, F1: " <<
            std::fixed << std::setprecision(4) << precision <<
            std::setprecision(6) << 
            " (" << num_match << "/" << num_prediction << ")" << ", " <<
            std::fixed << std::setprecision(4) << recall <<
            std::setprecision(6) <<
            " (" << num_match << "/" << num_reference << ")" << ", " <<
            std::fixed << std::setprecision(4) << f1score << std::endl;
        os << std::setprecision(6);
        os.unsetf(std::ios::fixed);
    }

    /**
     * Outputs macro-average precision, recall, F1 scores.
     *  @param  os          The output stream.
     *  @param  pb          The iterator for the first element of the
     *                      positive labels.
     *  @param  pe          The iterator just beyond the last element
     *                      of the positive labels.
     */
    template <class positive_iterator_type>
    void output_macro(
        std::ostream& os,
        positive_iterator_type pb,
        positive_iterator_type pe
        ) const
    {
        int n = 0;
        double precision = 0., recall = 0., f1 = 0.;

        for (positive_iterator_type it = pb;it != pe;++it) {
            if (0 < m_stat[*it].num_prediction || 0 < m_stat[*it].num_reference) {
                double p = divide(m_stat[*it].num_match, m_stat[*it].num_prediction);
                double r = divide(m_stat[*it].num_match, m_stat[*it].num_reference);
                double f = divide(2 * p * r, p + r);
                precision += p;
                recall += r;
                f1 += f;
                ++n;
            }
        }

        if (0 < n) {
            precision /= n;
            recall /= n;
            f1 /= n;
        }

        os << "Macro P, R, F1: " << 
            std::fixed << std::setprecision(4) << precision << ", " <<
            std::fixed << std::setprecision(4) << recall << ", " <<
            std::fixed << std::setprecision(4) << f1 << std::endl;
        os << std::setprecision(6);
        os.unsetf(std::ios::fixed);
    }

private:
    template <typename value_type>
    static inline double divide(value_type a, value_type b)
    {
        return (b != 0) ? (a / (double)b) : 0.;
    }
};



/**
 * Hold-out evaluation for binary classification.
 *  @param  os              The output stream.
 *  @param  first           The iterator pointing to the first element of the
 *                          dataset.
 *  @param  last            The iterator pointing just beyond the last element
 *                          of the dataset.
 *  @param  cls             The classifier object.
 *  @param  holdout         The group number for holdout evaluation.
 */
template <
    class iterator_type,
    class classifier_type
>
static void holdout_evaluation_binary(
    std::ostream& os,
    iterator_type first,
    iterator_type last,
    classifier_type& cls,
    int holdout
    )
{
    accuracy acc;
    precall pr(2);
    static const int positive_labels[] = {1};

    // For each instance in the data.
    for (iterator_type it = first;it != last;++it) {
        // Skip instances for training.
        if (it->get_group() != holdout) {
            continue;
        }

        // Compute the score for the instance.
        cls.inner_product(it->begin(), it->end());
        int rl = static_cast<int>(it->get_label());
        int ml = static_cast<int>(static_cast<bool>(cls));

        // Store the results.
        acc.set(ml == rl);
        pr.set(ml, rl);
    }

    acc.output(os);
    pr.output_micro(os, positive_labels, positive_labels+1);
}



/**
 * Hold-out evaluation for multi-class classification.
 *  @param  os              The output stream.
 *  @param  first           The iterator pointing to the first element of the
 *                          dataset.
 *  @param  last            The iterator pointing just beyond the last element
 *                          of the dataset.
 *  @param  cls             The classifier object.
 *  @param  fgen            The feature generator.
 *  @param  holdout         The group number for holdout evaluation.
 *  @param  label_first     The iterator pointing to the first element of the
 *                          set of positive labels.
 *  @param  label_last      The iterator pointing just beyond the last element
 *                          of the set of positive labels.
 */
template <
    class iterator_type,
    class classifier_type,
    class feature_generator_type,
    class labels_type,
    class label_iterator_type
>
static void holdout_evaluation_multi(
    std::ostream& os,
    iterator_type first,
    iterator_type last,
    classifier_type& cls,
    feature_generator_type& fgen,
    int holdout,
    bool acconly,
    const labels_type& labels,
    label_iterator_type label_first,
    label_iterator_type label_last
    )
{
    const int L = fgen.num_labels();
    accuracy acc;
    precall pr(L);

    // For each instance in the data.
    for (iterator_type it = first;it != last;++it) {
        // Exclude instances for holdout evaluation.
        if (it->get_group() != holdout) {
            continue;
        }

        // Tell the classifier the number of possible labels.
        cls.resize(it->num_candidates(L));

        for (int i = 0;i < it->num_candidates(L);++i) {
            cls.inner_product(
                i,
                fgen,
                it->attributes(i).begin(),
                it->attributes(i).end(),
                i
                );
        }
        cls.finalize();

        int argmax = cls.argmax();
        acc.set(argmax == it->get_label());
        if (!acconly) {    
            pr.set(argmax, it->get_label());
        }
    }

    // Report accuracy, precision, recall, and f1 score.
    acc.output(os);
    if (!acconly) {
        pr.output_labelwise(os, labels, label_first, label_last);
        pr.output_micro(os, label_first, label_last);
        pr.output_macro(os, label_first, label_last);
    }
}

};

#endif/*__CLASSIAS_EVALUATION_H__*/
