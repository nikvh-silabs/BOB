//
//  client.cpp
//
//  Copyright (c) 2019 Yuji Hirose. All rights reserved.
//  MIT License
//

#include <httplib.h>
#include <iostream>

//#pragma comment(lib, "crypt32")
//#pragma comment(lib, "ws2_32.lib")

#define CA_CERT_FILE "./ca-bundle.crt"

using namespace std;

int main(void) {
#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
  //httplib::SSLClient cli("stash.silabs.com", 443);
  // httplib::SSLClient cli("google.com");
   httplib::SSLClient cli("www.youtube.com");
  //cli.set_ca_cert_path(CA_CERT_FILE);
  //cli.enable_server_certificate_verification(true);
#else
  httplib::Client cli("192.168.1.153", 3000);
#endif

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

  return 0;
}
