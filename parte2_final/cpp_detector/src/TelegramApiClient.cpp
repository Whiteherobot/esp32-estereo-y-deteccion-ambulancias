#include "TelegramApiClient.hpp"
#include <curl/curl.h>
#include <utility>

TelegramApiClient::TelegramApiClient(std::string endpoint)
    : endpoint_(std::move(endpoint)) {
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

bool TelegramApiClient::sendAlert(const std::string& imagePath,
                                  const std::string& videoPath,
                                  double confidence) {
    CURL* curl = curl_easy_init();
    if (!curl)
        return false;

    curl_mime* mime = curl_mime_init(curl);

    curl_mimepart* part = curl_mime_addpart(mime);
    curl_mime_name(part, "image");
    curl_mime_filedata(part, imagePath.c_str());

    if (!videoPath.empty()) {
        part = curl_mime_addpart(mime);
        curl_mime_name(part, "video");
        curl_mime_filedata(part, videoPath.c_str());
    }

    part = curl_mime_addpart(mime);
    curl_mime_name(part, "confidence");
    std::string conf = std::to_string(confidence);
    curl_mime_data(part, conf.c_str(), CURL_ZERO_TERMINATED);

    curl_easy_setopt(curl, CURLOPT_URL, endpoint_.c_str());
    curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 180L);

    CURLcode rc = curl_easy_perform(curl);
    long code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);

    curl_mime_free(mime);
    curl_easy_cleanup(curl);
    return rc == CURLE_OK && code >= 200 && code < 300;
}
