#ifdef  HAVE_CONFIG_H
#include <config.h>
#endif/*HAVE_CONFIG_H*/

#include <iostream>
#include "option.h"
#include "optparse.h"
#include "util.h"
#include "tokenize.h"

#define	APPLICATION_S	"Classias"
#define	VERSION_S		"0.1"
#define	COPYRIGHT_S		"Copyright (c) 2008 Naoaki Okazaki"

int selector_train(option& opt);
int multiclass_train(option& opt);
int ranker_train(option& opt);
int biclass_train(option& opt);

class optionparser : public option, public optparse
{
public:
    optionparser(
        std::istream& _is = std::cin,
        std::ostream& _os = std::cout,
        std::ostream& _es = std::cerr
        ) : option(_is, _os, _es)
    {
    }

    BEGIN_OPTION_MAP_INLINE()
        ON_OPTION(SHORTOPT('l') || LONGOPT("learn"))
            mode = MODE_TRAIN;

        ON_OPTION(SHORTOPT('t') || LONGOPT("tag"))
            mode = MODE_TAG;

        ON_OPTION(SHORTOPT('h') || LONGOPT("help"))
            mode = MODE_HELP;

        ON_OPTION_WITH_ARG(SHORTOPT('f') || LONGOPT("task"))
            if (strcmp(arg, "binary") == 0 || strcmp(arg, "b") == 0) {
                type = TYPE_BICLASS;
            } else if (strcmp(arg, "multiclass") == 0 || strcmp(arg, "m") == 0) {
                type = TYPE_MULTICLASS;
            } else if (strcmp(arg, "selection") == 0 || strcmp(arg, "s") == 0) {
                type = TYPE_SELECTOR;
            } else if (strcmp(arg, "ranking") == 0 || strcmp(arg, "r") == 0) {
                type = TYPE_RANKER;
            } else {
                std::stringstream ss;
                ss << "unknown task type specified: " << arg;
                throw invalid_value(ss.str());
            }

        ON_OPTION_WITH_ARG(SHORTOPT('m') || LONGOPT("model"))
            model = arg;

        ON_OPTION_WITH_ARG(SHORTOPT('n') || LONGOPT("negative"))
            negatives.clear();
            std::string labels = arg;
            tokenizer field(labels, ' ');
            while (field.next()) {
                if (!field->empty()) {
                    negatives.insert(*field);
                }
            }

        ON_OPTION_WITH_ARG(SHORTOPT('a') || LONGOPT("algorithm"))
            if (strcmp(arg, "MaxEnt") == 0) {
            } else {
                std::stringstream ss;
                ss << "unknown training algorithm specified: " << arg;
                throw invalid_value(ss.str());
            }
            algorithm = arg;

        ON_OPTION_WITH_ARG(SHORTOPT('p') || LONGOPT("set"))
            params.push_back(arg);

        ON_OPTION_WITH_ARG(SHORTOPT('g') || LONGOPT("split"))
            split = atoi(arg);

        ON_OPTION_WITH_ARG(SHORTOPT('e') || LONGOPT("holdout"))
            holdout = atoi(arg);

        ON_OPTION(SHORTOPT('x') || LONGOPT("cross-validate"))
            cross_validation = true;

    END_OPTION_MAP()
};

static void usage(std::ostream& os, const char *argv0)
{
    os << "USAGE: " << argv0 << " [OPTIONS] [DATA1] [DATA2] ..." << std::endl;
    os << "  DATA    file(s) corresponding to a data set for training or tagging;" << std::endl;
    os << "          if multiple N files are specified, this tool assumes a data set to" << std::endl;
    os << "          be split into N groups and assigns a group number (1...N) to the" << std::endl;
    os << "          instances in each file; if no file is specified, the tool reads a" << std::endl;
    os << "          data set from STDIN" << std::endl;
    os << std::endl;
    os << "COMMANDS:" << std::endl;
    os << "  -l, --learn           train a model from the training set" << std::endl;
    os << "  -t, --tag             tag the data with the model (specified by -m option)" << std::endl;
    os << "  -h, --help            show the help message and exit" << std::endl;
    os << std::endl;
    os << "COMMON OPTIONS:" << std::endl;
    os << "  -f, --task=TYPE       specify a task type (DEFAULT='multiclass'):" << std::endl;
    os << "      b, binary             an instance is represented by an attribute vector," << std::endl;
    os << "                            which is identical to a feature vector;" << std::endl;
    os << "                            an instance label is boolean, 0 or 1;" << std::endl;
    os << "      m, multiclass         an instance is represented by an attribute vector;" << std::endl;
    os << "                            an instance label is chosen from all of the labels" << std::endl;
    os << "                            found in the training set; features are represented" << std::endl;
    os << "                            by Cartesian products of attributes and labels" << std::endl;
    os << "      s, selection          this is identical to 'multiclass' except that" << std::endl;
    os << "                            an instance label is chosen from candidate labels" << std::endl;
    os << "                            specified for each instance" << std::endl;
    os << "      r, ranking            an instance consists of candidates each of which" << std::endl;
    os << "                            has an attribute vector; features are identical" << std::endl;
    os << "                            to attributes" << std::endl;
    os << "  -m, --model=FILE      store/load a model to/from FILE (DEFAULT='')" << std::endl;
    os << "  -n, --negative=LABELS assume LABELS as negative labels (DEFAULT='-1 O')" << std::endl;
    os << std::endl;
    os << "TRAINING OPTIONS:" << std::endl;
    os << "  -a, --algorithm=NAME  specify a training algorithm (DEFAULT='maxent')" << std::endl;
    os << "      maxent                maximum entropy modeling (for MSR)" << std::endl;
    os << "      logress               logistic regression (for B)" << std::endl;
//    os << "      MLE                   maximum likelihood estimation (MLE) without" << std::endl;
//    os << "                            regularization" << std::endl;
//    os << "      L1                    maximum a posterior (MAP) estimation with L1 norm" << std::endl;
//    os << "                            of parameters (L1 regularization; Laplacian prior)" << std::endl;
//    os << "      L2                    maximum a posterior (MAP) estimation with L2 norm" << std::endl;
//    os << "                            of parameters (L2 regularization; Gaussian prior)" << std::endl;
    os << "  -p, --set=NAME=VALUE  set the algorithm-specific parameter NAME to VALUE" << std::endl;
    os << "  -g, --split=N         split the instances into N groups; this option is" << std::endl;
    os << "                        useful for holdout evaluation and cross validation" << std::endl;
    os << "  -e, --holdout=M       use the M-th data for holdout evaluation and the rest" << std::endl;
    os << "                        for training" << std::endl;
    os << "  -x, --cross-validate  repeat holdout evaluations for M in {1, ..., N}" << std::endl;
    os << "                        (N-fold cross validation)" << std::endl;
    os << std::endl;
}


int main(int argc, char *argv[])
{
    int ret = 0;
    int arg_used = 0;
    optionparser opt;
    std::istream& is = opt.is;
    std::ostream& os = opt.os;
    std::ostream& es = opt.es;

    // Show the copyright information.
    es << APPLICATION_S " " VERSION_S "  " COPYRIGHT_S << std::endl;
    es << std::endl;

    // Parse the command-line options.
    try { 
        arg_used = opt.parse(argv, argc);
    } catch (const optparse::unrecognized_option& e) {
        es << "ERROR: unrecognized option: " << e.what() << std::endl;
        return 1;
    } catch (const optparse::invalid_value& e) {
        es << "ERROR: " << e.what() << std::endl;
        return 1;
    }

    // Show the help message and exit.
    if (opt.mode == option::MODE_HELP) {
        usage(os, argv[0]);
        return ret;
    }

    // Set the source files.
    for (int i = arg_used;i < argc;++i) {
        opt.files.push_back(argv[i]);
    }

    // Branch for tasks.
    try {
        if (opt.mode == option::MODE_TRAIN) {
            switch (opt.type) {
            case option::TYPE_BICLASS:
                //ret = biclass_train(opt);
                break;
            case option::TYPE_MULTICLASS:
                ret = multiclass_train(opt);
                break;
            case option::TYPE_SELECTOR:
                ret = selector_train(opt);
                break;
            case option::TYPE_RANKER:
                ret = ranker_train(opt);
                break;
            }
        } else if (opt.mode == option::MODE_TAG) {

        }
    } catch (const invalid_data& e) {
        es << "ERROR: " << e.what() << std::endl;
        return 1;
    }

    return ret;
}