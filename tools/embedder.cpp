#include <cstddef>
#include <fstream>
#include <ios>
#include <iostream>
#include <filesystem>
#include <sstream>

std::string toHex(const char &inp_char){
    std::string result;
    static const char alphabet[] = "0123456789abcdef";

    result.push_back(alphabet[(inp_char>>4) & 0x0F]);
    result.push_back(alphabet[inp_char & 0x0F]);

    return result;
}

std::string prcess(std::filesystem::path &f) {
    std::string result;
    std::stringstream s;
    s << f.filename();

    result = s.str();
    result.erase(result.begin());
    result.erase(result.end() - 1);
    if (size_t pos = result.find("."); pos != std::string::npos) {
        result[pos] = '_';
    }

    return result;
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: embedder [path_to_shader.glsl] [path_to_out_header.hpp]\n";
        return 1;
    }

    std::filesystem::path input_path(argv[1]);
    std::filesystem::path output_path(argv[2]);

    if(!std::filesystem::exists(input_path)) {
        std::cerr << "Unable to find glsl shader file: " << input_path << std::endl;
        return 1;
    }

    std::ifstream inp(input_path, std::ios_base::binary);
    std::ofstream out(output_path, std::ios_base::binary);

    if(!inp.is_open() || !out.is_open()) {
        std::cerr << "Unable to open input (" << input_path << ") and output (" << output_path << ") files\n";
        return 1;
    }


    std::stringstream ss_input;
    ss_input << inp.rdbuf();
    std::string file_var_name = prcess(input_path);

    out << "unsigned char " << file_var_name << "[] = {\n";

    std::string s = ss_input.str();

    for (int i = 0; i < s.length(); i++) {
        if (i == s.length() - 1) {
            out << "0x" << toHex(s[i]);
        } else {
            out << "0x" << toHex(s[i]) << ", ";
        }
    }

    out << " };\n";
    out << "unsigned int " << file_var_name << "_len = " << s.length() << ";";

    ss_input.clear();
    inp.close();
    out.close();
    return 0;
}
