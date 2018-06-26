#include <boost/program_options.hpp>
#include <cortono/http/app.hpp>
#include </usr/local/include/inja.hpp>

namespace fs = std::experimental::filesystem;

std::string render_view(const std::string& filepath, const std::unordered_map<std::string, inja::json>& m) {
    inja::Environment env = inja::Environment("./www/");
    env.set_element_notation(inja::ElementNotation::Dot);
    inja::Template tmpl = env.parse_template(filepath);
    inja::json tmpl_json_data;
    for(auto& [key, value] : m) {
        tmpl_json_data[key] = value;
    }
    for(auto& [key, data] : m) {
        if(data.is_object()) {
            for(auto it = data.begin(); it != data.end(); ++it) {
                tmpl_json_data[it.key()] = it.value();
            }
        }
    }
    return env.render_template(tmpl, tmpl_json_data);
} 

std::string parse_file_name(const std::string& filepath) {
    auto pos = filepath.find_last_of('/');
    std::string filename;
    if(pos != std::string::npos)
        filename = filepath.substr(pos + 1);
    else 
        filename = filepath;
    log_info(filename);
    return filename;
}
cortono::http::Response handle_directory(const std::string& directory, const std::string& host, unsigned short port) {
    log_info(host, port);
    inja::json files;
    for(auto& p : fs::directory_iterator(directory)) {
        inja::json file;
        std::stringstream oss;
        oss << p;
        std::string filepath = oss.str().substr(1, oss.str().size() - 2);
        file["path"] = filepath;
        file["name"] = parse_file_name(filepath);
        if(fs::is_directory(p)) {
            file["icon"] = "img/dir.png";
        }
        else {
            file["icon"] = "img/file.png";
        }
        file["host"] = host;
        file["port"] = port;
        files.push_back(file);
    }
    std::unordered_map<std::string, inja::json> m;
    m["files"] = files;
    std::string s = render_view("template.html", m);
    log_info(s);
    return cortono::http::Response(std::move(s));
}
int main()
{
    namespace po = boost::program_options;

    po::options_description parser("param parser");
    parser.add_options()
        ("host", po::value<std::string>(), "set host")
        ("port", po::value<unsigned short>(), "set port")
        ("https", po::value<bool>(), "enable https")
        ("directory", po::value<std::string>(), "set file directory")
        ;
    po::variables_map vm;
    try {
        std::ifstream fin{ "server.conf" };
        po::store(po::parse_config_file(fin, parser), vm);
        po::notify(vm);
        fin.close();

        if(!vm.count("host"))
            log_fatal("config error: no host");
        if(!vm.count("port"))
            log_fatal("config error: no port");
        if(!vm.count("directory"))
            log_fatal("config error: no directory");
        std::string ip = vm["host"].as<std::string>();
        unsigned short port = vm["port"].as<unsigned short>();
        std::string directory = vm["directory"].as<std::string>();

        log_info(ip, port, directory);
        using namespace cortono;
        http::SimpleApp app;
        app.register_rule("/" + directory)([&](const http::Request& , http::Response& res) {
            res = handle_directory(directory, ip, port);
        });
        app.register_rule("/" + directory + "<path>")([&](const http::Request&, http::Response& res, std::string filename) {
            log_info(filename);
            std::string filepath = directory + filename;
            if(fs::is_directory(filepath)) {
                res = handle_directory(filepath, ip, port);
            }
            else {
                res.send_file(filepath);
                auto pos = filepath.find_last_of('/');
                if(pos != std::string::npos)
                    filename = filepath.substr(pos + 1);
                res.set_header("Content-Disposition", "attachment;filename="+filename);
            }
        });
        app.register_rule("/img/<path>")([&](const http::Request& , http::Response& res, std::string filename) {
            log_info(filename);
            res.send_file("img/" + filename);
            return; 
        });
        app.register_rule("/" + directory + "upload").methods(http::HttpMethod::POST)([&](const http::Request& req) {
            if(req.upload_files.empty()) 
                return "error";
            std::string upload_path = directory + "upload/";
            if(!fs::exists(upload_path)) 
                fs::create_directory(upload_path);

            for(auto& upload_file : req.upload_files) {
                log_info("start write file:", upload_path, upload_file.filename);
                std::ofstream fout{ upload_path + upload_file.filename, std::ios_base::out };
                fout.write(upload_file.content.data(), upload_file.content.length());
                fout.close();
            }
            return "ok";
        });
        app.bindaddr(ip).port(port).multithread().run();
    }
    catch(...) {
        std::cout << "error" << std::endl;
    }
    return 0;

}
