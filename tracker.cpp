#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cstdlib>
#include <curl/curl.h>
#include "json.hpp"

using json = nlohmann::json;

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* s) {
    size_t totalSize = size * nmemb;
    s->append((char*)contents, totalSize);
    return totalSize;
}

std::string makeRequest(const std::string& url, const std::string& apiKey, const std::string& method = "GET", const std::string& body = "") {
    CURL* curl = curl_easy_init();
    std::string response;

    if (curl) {
        struct curl_slist* headers = NULL;
        headers = curl_slist_append(headers, ("X-API-Key: " + apiKey).c_str());
        headers = curl_slist_append(headers, "Content-Type: application/json");

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

        if (method == "POST") {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
        }

        CURLcode res = curl_easy_perform(curl);

        if (res != CURLE_OK) {
            std::cerr << "CURL error: " << curl_easy_strerror(res) << "\n";
        }

        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }

    return response;
}

int main() {

#ifdef BUNGIE_API_KEY
    std::string apiKey = BUNGIE_API_KEY;
#else
    const char* key = std::getenv("BUNGIE_API_KEY");
    if (!key) {
        std::cerr << "Set BUNGIE_API_KEY environment variable.\n";
        return 1;
    }
    std::string apiKey = key;
#endif

    std::string input;
    std::cout << "Enter Bungie name (e.g. char#5202): ";
    std::getline(std::cin, input);

    size_t hashPos = input.find('#');
    if (hashPos == std::string::npos) {
        std::cerr << "Invalid format.\n";
        return 1;
    }

    std::string displayName = input.substr(0, hashPos);
    int displayCode = std::stoi(input.substr(hashPos + 1));

    std::vector<int> platforms = {3, 1, 2, 6};
    std::string membershipId;
    int membershipType = -1;
    int crossSaveOverride = 0;

    for (int platform : platforms) {

        std::string url =
            "https://www.bungie.net/Platform/Destiny2/SearchDestinyPlayerByBungieName/" +
            std::to_string(platform) + "/";

        json body = {
            {"displayName", displayName},
            {"displayNameCode", displayCode}
        };

        std::string response = makeRequest(url, apiKey, "POST", body.dump());
        json j = json::parse(response);

        if (j.contains("Response") && j["Response"].is_array() && !j["Response"].empty()) {
            membershipId = j["Response"][0]["membershipId"].get<std::string>();
            membershipType = j["Response"][0]["membershipType"].get<int>();
            crossSaveOverride = j["Response"][0]["crossSaveOverride"].get<int>();
            break;
        }
    }

    if (membershipId.empty()) {
        std::cerr << "Player not found.\n";
        return 1;
    }

    std::cout << "Found membershipId: " << membershipId << "\n";
    std::cout << "Found membershipType: " << membershipType << "\n";
    std::cout << "crossSaveOverride: " << crossSaveOverride << "\n";

    if (crossSaveOverride != 0 && crossSaveOverride != membershipType) {
        std::cout << "Cross-save detected, resolving primary account...\n";

        std::string linkedUrl =
            "https://www.bungie.net/Platform/Destiny2/" +
            std::to_string(membershipType) +
            "/Profile/" + membershipId +
            "/LinkedProfiles/";

        std::string linkedRaw = makeRequest(linkedUrl, apiKey);
        json linked = json::parse(linkedRaw);

        if (linked.contains("Response") && linked["Response"].contains("profiles")) {
            bool resolved = false;
            for (auto& profile : linked["Response"]["profiles"]) {
                int profType = profile["membershipType"].get<int>();
                if (profType == crossSaveOverride) {
                    membershipId = profile["membershipId"].get<std::string>();
                    membershipType = profType;
                    resolved = true;
                    std::cout << "Resolved to primary membershipId: " << membershipId << "\n";
                    std::cout << "Resolved to primary membershipType: " << membershipType << "\n";
                    break;
                }
            }
            if (!resolved) {
                std::cerr << "Warning: could not resolve cross-save override, proceeding with original.\n";
            }
        } else {
            std::cerr << "Warning: LinkedProfiles call failed, proceeding with original.\n";
        }
    }

    std::string profileUrl =
        "https://www.bungie.net/Platform/Destiny2/" +
        std::to_string(membershipType) +
        "/Profile/" + membershipId +
        "/?components=100,200";

    std::string profileRaw = makeRequest(profileUrl, apiKey);
    json profile = json::parse(profileRaw);

    if (profile.contains("ErrorStatus") && profile["ErrorStatus"] != "Success") {
        std::cerr << "Bungie API error: " << profile["Message"] << "\n";
        std::cerr << profile.dump(2) << "\n";
        return 1;
    }

    if (!profile.contains("Response") ||
        !profile["Response"].contains("characters") ||
        !profile["Response"]["characters"].contains("data"))
    {
        std::cerr << "Failed to retrieve characters.\n";
        std::cerr << profile.dump(2) << "\n";
        return 1;
    }

    auto characters = profile["Response"]["characters"]["data"];

    if (characters.empty()) {
        std::cerr << "No characters found on this account.\n";
        return 1;
    }

    std::cout << "Found " << characters.size() << " character(s).\n";

    std::ofstream file("activities.csv");
    file << "instanceId,mode,date,durationSeconds,pgcrLink\n";

    int activityCount = 0;

    for (auto& [charId, _] : characters.items()) {

        std::cout << "Fetching activities for character: " << charId << "\n";

        int page = 0;

        while (true) {

            std::string activitiesUrl =
                "https://www.bungie.net/Platform/Destiny2/" +
                std::to_string(membershipType) +
                "/Account/" + membershipId +
                "/Character/" + charId +
                "/Stats/Activities/?count=250&page=" + std::to_string(page);

            std::string actRaw = makeRequest(activitiesUrl, apiKey);
            json activities = json::parse(actRaw);

            if (!activities.contains("Response") ||
                !activities["Response"].contains("activities") ||
                activities["Response"]["activities"].empty())
            {
                std::cout << "No more activities for character " << charId << " at page " << page << "\n";
                break;
            }

            for (auto& act : activities["Response"]["activities"]) {
                std::string instanceId = act["activityDetails"]["instanceId"].get<std::string>();

                file << instanceId << ",";
                file << act["activityDetails"]["mode"] << ",";
                file << act["period"] << ",";
                file << act["values"]["activityDurationSeconds"]["basic"]["value"] << ",";
                file << "https://www.bungie.net/en/PGCR/" << instanceId << "\n";

                activityCount++;
            }

            std::cout << "  Page " << page << ": fetched " << activities["Response"]["activities"].size() << " activities (total: " << activityCount << ")\n";

            page++;
        }
    }

    file.close();

    if (activityCount == 0) {
        std::cout << "No activities found.\n";
    } else {
        std::cout << "Saved " << activityCount << " activities to activities.csv\n";
    }

    return 0;
}