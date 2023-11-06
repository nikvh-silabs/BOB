//
//  client.cpp
//
//  Copyright (c) 2019 Yuji Hirose. All rights reserved.
//  MIT License
//

#define CPPHTTPLIB_OPENSSL_SUPPORT

#include <filesystem>
#include <httplib.h>
#include <iostream>

#define CA_CERT_FILE "./ca-bundle.crt"

using namespace std;

int main(void) {
#ifdef CPPHTTPLIB_OPENSSL_SUPPORT

  // httplib::SSLClient cli("stash.silabs.com", 443);
  //  httplib::SSLClient cli("google.com");
  std::filesystem::remove("latest-yakka.exe");
  auto fp = fopen("latest-yakka.exe", "wb");
  if (fp == NULL) { exit(-1); }

  std::cout << "Connecting to github\r\n";

  // https://developer.arm.com/

  std::string url = "/-/media/Files/downloads/gnu/13.2.rel1/binrel/"
                    "arm-gnu-toolchain-13.2.rel1-mingw-w64-i686-arm-none-eabi."
                    "zip?rev=93fda279901c4c0299e03e5c4899b51f&hash="
                    "A3C5FF788BE90810E121091C873E3532336C8D46";

  httplib::SSLClient client("developer.arm.com");
  // client.enable_server_certificate_verification(false);
  // client.set_ca_cert_path(CA_CERT_FILE);
  client.set_follow_location(true);

  std::cout << "Getting file\r\n";

  // auto result = client.Get(
  //     "/github-production-release-asset-2e65be/416498379/"
  //     "4843c680-4206-494f-b05a-6bd0946d3f0b?X-Amz-Algorithm=AWS4-HMAC-SHA256&X-"
  //     "Amz-Credential=AKIAIWNJYAX4CSVEH53A%2F20231031%2Fus-east-1%2Fs3%2Faws4_"
  //     "request&X-Amz-Date=20231031T012832Z&X-Amz-Expires=300&X-Amz-Signature="
  //     "d563bb1a256b88a091a9492486d4c1cacd38037986566ce8bcf29f17e54f1147&X-Amz-"
  //     "SignedHeaders=host&actor_id=0&key_id=0&repo_id=416498379&response-"
  //     "content-disposition=attachment%3B%20filename%3Dyakka-linux&response-"
  //     "content-type=application%2Foctet-stream");
  auto result = client.Get(
      url,
      [&](const char *data, size_t data_length) -> bool {
        fwrite(data, data_length, sizeof(std::byte), fp);
        return true;
      },
      [&](uint64_t current, uint64_t total) -> bool { std::cout << "."; });
  if (result != nullptr) {
    std::cout << result->status << "\r\n"
              << result->get_header_value("Content-Type") << "\r\n";
    for (auto h : result->headers) {
      std::cout << h.first << " : " << h.second << "\r\n";
    }
    std::cout << result->body << "\r\n";
  } else {
    auto openssl_result = client.get_openssl_verify_result();
    if (openssl_result) {
      cout << "verify error: " << X509_verify_cert_error_string(openssl_result)
           << endl;
    }

    std::cout << "Failed to get\r\n";
  }
  client.stop();
  fclose(fp);
  // cli.set_ca_cert_path(CA_CERT_FILE);
#else
  httplib::Client cli("192.168.1.153", 3000);

  if (auto res = cli.Get("/")) {
    cout << res->status << endl;
    cout << res->get_header_value("Content-Type") << endl;
    cout << res->body << endl;
  } else {
    cout << "error code: " << res.error() << std::endl;
#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
    auto result = cli.get_openssl_verify_result();
    if (result) {
      cout << "verify error: " << X509_verify_cert_error_string(result) << endl;
    }
#endif
  }
#endif

  return 0;
}
