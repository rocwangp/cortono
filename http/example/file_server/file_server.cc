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
    if(pos != std::string::npos)
        return filepath.substr(pos + 1);
    else 
        return filepath;
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
        std::ifstream fin{ "server.conf "};
        po::store(po::parse_config_file(fin, parser), vm);
        po::notify(vm);
        fin.close();

        std::string directory{ "/download" };
        std::string ip{ "127.0.0.1" };
        unsigned short port{ 9999 };
        if(vm.count("host"))
            ip = vm["host"].as<std::string>();
        if(vm.count("port"))
            port = vm["port"].as<unsigned short>();
        if(vm.count("directory"))
            directory = vm["directory"].as<std::string>();

        log_info(ip, port);
        std::string pre_dir = directory;
        if(pre_dir.front() == '/')
            pre_dir = directory.substr(1);
        if(pre_dir.back() != '/')
            pre_dir.append(1, '/');


        using namespace cortono;
        http::SimpleApp app;
        app.register_rule(directory)([&, ip, port](const http::Request& , http::Response& res) {
            res = handle_directory(pre_dir, ip, port);
            return;
        });
        app.register_rule(directory + "/<path>")([&, ip, port](const http::Request&, http::Response& res, std::string filename) {
            log_info(filename);
            if(fs::is_directory(pre_dir + filename)) {
                res = handle_directory(pre_dir + filename, ip, port);
            }
            else if(fs::is_regular_file(pre_dir + filename)) {
                res.send_file(pre_dir + filename);
                auto pos = filename.find_last_of('/');
                if(pos != std::string::npos)
                    filename = filename.substr(pos + 1);
                res.set_header("Content-Disposition", "attachment;filename="+filename);
            }
        });
        app.register_rule("/img/<path>")([&](const http::Request& , http::Response& res, std::string filename) {
            log_info(filename);
            res.send_file("img/" + filename);
            return;
        });
        app.register_rule("/upload").methods(http::HttpMethod::POST)([&](const http::Request& req) {
            auto it = req.upload_kv_pairs.find("Content-Disposition");
            if(it == req.upload_kv_pairs.end())
                return "error";
            auto dispositions = http::utils::split(it->second, "; ");
            auto filename_sv = http::utils::split(dispositions[2], "=")[1];
            std::string filename(filename_sv.data(), filename_sv.length());

            if(filename.front() == '\"')
                filename = filename.substr(1);
            if(filename.back() == '\"')
                filename.pop_back();
            log_info(filename);

            std::ofstream fout{ "download/upload/" + filename, std::ios_base::out };
            fout.write(req.body.data(), req.body.size());
            fout.close();
            return "ok";
        });
        app.bindaddr(std::move(ip)).port(port).multithread().run();
    }
    catch(...) {
        std::cout << "error" << std::endl;
    }
    return 0;

}
