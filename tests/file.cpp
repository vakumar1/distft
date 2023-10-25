#include "tests/tests.h"

#include "src/client/file.h"
#include "src/dht/session.h"

#include <spdlog/spdlog.h>
#include <fstream>
#include <filesystem>

std::function<bool()> server_static_files(unsigned int num_servers, unsigned int num_files, size_t file_size, 
                                          unsigned int found_tol, unsigned int corr_tol) {
  
  auto fn = [num_servers, num_files, file_size, found_tol, corr_tol]() {
    // setup cluster
    Session* sessions[num_servers];
    std::vector<std::thread*> threads;
    for (int i = 0; i < num_servers; i++) {
      std::thread* session_thread = new std::thread(create_session, std::ref(sessions[i]), i, (i + 1) % num_servers);
      threads.push_back(session_thread);
    }
    wait_on_threads(threads);

    // create random files and add files to cluster
    std::filesystem::path base_path("/tmp");
    std::vector<std::string> test_files;
    for (int i = 0; i < num_files; i++) {
      test_files.push_back(std::to_string(i));
    }
    init_index_file(sessions[std::rand() % num_servers]);
    add_files_to_index_file(sessions[std::rand() % num_servers], test_files);
    for (int i = 0; i < num_files; i++) {
      random_file(base_path / std::to_string(i), file_size);
      write_from_file(sessions[std::rand() % num_servers], base_path / std::to_string(i), std::to_string(i));
    }

    // check cluster for files and compare to base file
    std::vector<std::vector<char>*> buffs;
    for (int i = 0; i < num_files; i++) {
      std::ifstream file(base_path / std::to_string(i), std::ios::binary);
      std::vector<char>* buff = new std::vector<char>(file_size);
      file.read(buff->data(), file_size);
      buffs.push_back(buff);
    }
    std::mutex data_lock;
    unsigned int num_found = 0;
    unsigned int num_correct = 0;
    for (int i = 0; i < num_files; i++) {
      threads.push_back(new std::thread(verify_file, sessions[std::rand() % num_servers], buffs[i], std::to_string(i),
                        std::ref(data_lock), std::ref(num_found), std::ref(num_correct)));
    }
    wait_on_threads(threads);

    // teardown cluster
    for (int i = 0; i < num_servers; i++) {
      threads.push_back(new std::thread([](Session*& s){
        s->teardown(false);
        delete s;
      }, std::ref(sessions[i])));
    }
    wait_on_threads(threads);
    for (int i = 0; i < num_files; i++) {
      delete buffs[i];
      std::remove((base_path / std::to_string(i)).c_str());
    }

    return (num_found >= num_files - found_tol) && (num_correct >= num_found - corr_tol);
  };
  return fn;
}

std::function<bool()> server_dynamic_files(unsigned int num_servers, unsigned int num_files, size_t file_size, 
                                            unsigned int found_tol, unsigned int corr_tol) {
  
  auto fn = [num_servers, num_files, file_size, found_tol, corr_tol](){
    // setup cluster
    Session* sessions[num_servers];
    std::vector<std::thread*> threads;
    for (int i = 0; i < num_servers; i++) {
      std::thread* session_thread = new std::thread(create_session, std::ref(sessions[i]), i, (i + 1) % num_servers);
      threads.push_back(session_thread);
    }
    wait_on_threads(threads);

    // create random files, add files to cluster, overwrite files
    std::filesystem::path base_path("/tmp");
    std::vector<std::string> test_files;
    for (int i = 0; i < num_files; i++) {
      test_files.push_back(std::to_string(i));
    }
    init_index_file(sessions[std::rand() % num_servers]);
    add_files_to_index_file(sessions[std::rand() % num_servers], test_files);
    for (int i = 0; i < num_files; i++) {
      threads.push_back(new std::thread([base_path, i, file_size, num_servers, &sessions]() {
        random_file(base_path / std::to_string(i), file_size);
        write_from_file(sessions[std::rand() % num_servers], base_path / std::to_string(i), std::to_string(i));
      }));
    }
    wait_on_threads(threads);
    for (int i = 0; i < num_files; i++) {
      threads.push_back(new std::thread([base_path, i, file_size, num_servers, &sessions]() {
        random_file(base_path / std::to_string(i), file_size);
        write_from_file(sessions[std::rand() % num_servers], base_path / std::to_string(i), std::to_string(i));
      }));
    }
    wait_on_threads(threads);

    // check cluster for files and compare to base file
    std::vector<std::vector<char>*> buffs;
    for (int i = 0; i < num_files; i++) {
      std::ifstream file(base_path / std::to_string(i), std::ios::binary);
      std::vector<char>* buff = new std::vector<char>(file_size);
      file.read(buff->data(), file_size);
      buffs.push_back(buff);
    }
    std::mutex data_lock;
    unsigned int num_found = 0;
    unsigned int num_correct = 0;
    for (int i = 0; i < num_files; i++) {
      threads.push_back(new std::thread(verify_file, sessions[std::rand() % num_servers], buffs[i], std::to_string(i),
                        std::ref(data_lock), std::ref(num_found), std::ref(num_correct)));
    }
    wait_on_threads(threads);

    // teardown cluster
    for (int i = 0; i < num_servers; i++) {
      threads.push_back(new std::thread([](Session*& s){
        s->teardown(false);
        delete s;
      }, std::ref(sessions[i])));
    }
    wait_on_threads(threads);
    for (int i = 0; i < num_files; i++) {
      delete buffs[i];
      std::remove((base_path / std::to_string(i)).c_str());
    }


    return (num_found >= num_files - found_tol) && (num_correct >= num_found - corr_tol);
  };
  return fn;
}