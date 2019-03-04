/**
 * @file
 * @brief Small converter for field data INIT <-> APF
 */

#include <algorithm>
#include <fstream>
#include <string>

#include "core/utils/log.h"
#include "tools/field_parser.h"
#include "tools/units.h"

using namespace allpix;

/**
 * @brief Main function running the application
 */
int main(int argc, const char* argv[]) {

    // Register the default set of units with this executable:
    add_units();

    // Add cout as the default logging stream
    Log::addStream(std::cout);
    add_units();

    // If no arguments are provided, print the help:
    bool print_help = false;
    int return_code = 0;
    if(argc == 1) {
        print_help = true;
        return_code = 1;
    }

    // Parse arguments
    FileType format_from;
    FileType format_to;
    std::string file_input;
    std::string file_output;
    bool scalar = false;
    for(int i = 1; i < argc; i++) {
        if(strcmp(argv[i], "-h") == 0) {
            print_help = true;
        } else if(strcmp(argv[i], "-v") == 0 && (i + 1 < argc)) {
            try {
                LogLevel log_level = Log::getLevelFromString(std::string(argv[++i]));
                Log::setReportingLevel(log_level);
            } catch(std::invalid_argument& e) {
                LOG(ERROR) << "Invalid verbosity level \"" << std::string(argv[i]) << "\", ignoring overwrite";
            }
        } else if(strcmp(argv[i], "--from") == 0 && (i + 1 < argc)) {
            std::string format = std::string(argv[++i]);
            std::transform(format.begin(), format.end(), format.begin(), ::tolower);
            format_from = (format == "init" ? FileType::INIT : format == "apf" ? FileType::APF : FileType::UNKNOWN);
        } else if(strcmp(argv[i], "--to") == 0 && (i + 1 < argc)) {
            std::string format = std::string(argv[++i]);
            std::transform(format.begin(), format.end(), format.begin(), ::tolower);
            format_to = (format == "init" ? FileType::INIT : format == "apf" ? FileType::APF : FileType::UNKNOWN);
        } else if(strcmp(argv[i], "--input") == 0 && (i + 1 < argc)) {
            file_input = std::string(argv[++i]);
        } else if(strcmp(argv[i], "--output") == 0 && (i + 1 < argc)) {
            file_output = std::string(argv[++i]);
        } else if(strcmp(argv[i], "--scalar") == 0) {
            scalar = true;
        } else {
            LOG(ERROR) << "Unrecognized command line argument \"" << argv[i] << "\"";
            print_help = true;
            return_code = 1;
        }
    }

    // Print help if requested or no arguments given
    if(print_help) {
        std::cout << "Allpix Squared Field Converter Tool" << std::endl;
        std::cout << std::endl;
        std::cout << "Usage: field_converter <parameters>" << std::endl;
        std::cout << std::endl;
        std::cout << "Parameters (all mandatory):" << std::endl;
        std::cout << "  --from <format>  file format of the input file" << std::endl;
        std::cout << "  --to <format>    file format of the output file" << std::endl;
        std::cout << "  --input <file>   input field file" << std::endl;
        std::cout << "  --output <file>  output field file" << std::endl << std::endl;
        std::cout << "Options:" << std::endl;
        std::cout << "  --scalar         Convert scalar field. Default is vector field" << std::endl;
        std::cout << std::endl;
        std::cout << "For more help, please see <https://cern.ch/allpix-squared>" << std::endl;
        return return_code;
    }

    try {
        FieldQuantity quantity = (scalar ? FieldQuantity::SCALAR : FieldQuantity::VECTOR);

        FieldParser<double> field_parser(quantity, "");
        auto field_data = field_parser.get_by_file_name(file_input, format_from);
        FieldWriter<double> field_writer(quantity, "");
        field_writer.write_file(field_data, file_output, format_to);
    } catch(std::exception& e) {
        LOG(FATAL) << "Fatal internal error" << std::endl << e.what() << std::endl << "Cannot continue.";
        return_code = 127;
    }

    return return_code;
}
