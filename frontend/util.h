#ifndef __ATTRIBUTE_H__
#define __ATTRIBUTE_H__

#include <fstream>
#include <ctime>
#include <sstream>
#include <string>
#include <stdexcept>

class invalid_data
{
protected:
    std::string message;

public:
    invalid_data(const char *const& msg, int lines)
    {
        std::stringstream ss;
        ss << "in lines " << lines << ", " << msg;
        message = ss.str();
    }

    invalid_data(const invalid_data& rho)
    {
        message = rho.message;
    }

    invalid_data& operator=(const invalid_data& rho)
    {
        message = rho.message;
    }

    virtual ~invalid_data()
    {
    }

    virtual const char *what() const
    {
        return message.c_str();
    }
};

class invalid_model
{
protected:
    std::string message;

public:
    invalid_model(const char *const& msg) : message(msg)
    {
    }

    invalid_model(const invalid_model& rho)
    {
        message = rho.message;
    }

    invalid_model& operator=(const invalid_model& rho)
    {
        message = rho.message;
    }

    virtual ~invalid_model()
    {
    }

    virtual const char *what() const
    {
        return message.c_str();
    }
};

class stopwatch
{
protected:
    clock_t begin;
    clock_t end;

public:
    stopwatch()
    {
        start();
    }

    void start()
    {
        begin = end = std::clock();
    }

    double stop()
    {
        end = std::clock();
        return get();
    }

    double get() const
    {
        return (end - begin) / (double)CLOCKS_PER_SEC;
    }
};

static void
get_name_value(
    const std::string& str, std::string& name, double& value)
{
    size_t col = str.rfind(':');
    if (col == str.npos) {
        name = str;
        value = 1.;
    } else {
        value = std::atof(str.c_str() + col + 1);
        name = str.substr(0, col);
    }
}

template <class data_type>
static void
read_data(
    data_type& data,
    const option& opt
    )
{
    std::ostream& os = std::cout;
    std::ostream& es = std::cerr;

    // Read files for training data.
    if (opt.files.empty()) {
        // Read the data from STDIN.
        os << "STDIN" << std::endl;
        read_stream(std::cin, data, 0);
    } else {
        // Read the data from files.
        for (int i = 0;i < (int)opt.files.size();++i) {
            std::ifstream ifs(opt.files[i].c_str());
            if (!ifs.fail()) {
                os << "File (" << i+1 << "/" << opt.files.size() << ") : " << opt.files[i] << std::endl;
                read_stream(ifs, data, i);
            }
            ifs.close();
        }
    }
}

template <class data_type>
static int
split_data(
    data_type& data,
    const option& opt
    )
{
    int i = 0;
    typename data_type::iterator it;
    for (it = data.begin();it != data.end();++it, ++i) {
        it->set_group(i % opt.split);
    }
    return opt.split;
}

template <class data_type>
static int
read_dataset(
    data_type& data,
    const option& opt
    )
{
    // Read the training data.
    read_data(data, opt);

    // Split the training data if necessary.
    if (0 < opt.split) {
        split_data(data, opt);
        return opt.split;
    } else {
        return (int)opt.files.size();
    }
}

template <class char_type, class traits_type>
inline std::basic_ostream<char_type, traits_type>&
timestamp(std::basic_ostream<char_type, traits_type>& os)
{
	time_t ts;
	time(&ts);

   	char str[80];
    std::strftime(
        str, sizeof(str), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&ts));

    os << str;
    return (os);
}

#if 0

template <class data_type, class feature_type>
static void
generate_features(
    feature_type& features,
    data_type& data,
    const size_t L
    )
{
    if (features.assign()) {
        typename data_type::const_iterator it;
        for (it = data.begin();it != data.end();++it) {
            for (size_t i = 0;i < it->num_candidates(L);++i) {
                int l = it->candidate(i);
                const typename data_type::attributes_type& cont = it->content(i);
                if (it->label == l) {
                    typename data_type::attributes_type::const_iterator itc;
                    for (itc = cont.begin();itc != cont.end();++itc) {
                        features.assign(itc->first, it->label);
                    }
                }
            }
        }
    }
}

template <class trainer_type>
inline int
train(trainer_type& trainer, const option& opt, int holdout = -1)
{
    // Set options.
    if (opt.algorithm == "L1") {
        trainer.set("sigma1", opt.sigma);
    } else if (opt.algorithm == "L2") {
        trainer.set("sigma2", opt.sigma);
    }

    // Start training.
    trainer.train(opt.os, holdout);

    return 0;
}

#endif

#endif/*__ATTRIBUTE_H__*/
