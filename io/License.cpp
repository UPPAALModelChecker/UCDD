// -*- mode: C++; c-file-style: "stroustrup"; c-basic-offset: 4; indent-tabs-mode: nil; -*-
////////////////////////////////////////////////////////////////////
//
// This file is a part of the UPPAAL toolkit.
// Copyright (c) 1995 - 2003, Uppsala University and Aalborg University.
// All right reserved.
//
///////////////////////////////////////////////////////////////////

#ifdef ENABLE_LICENSE

#include "io/Connection.h"
#include "io/License.h"
#include "io/Base64Coder.h"

#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <cstring>
#include <ctime>
#include <cassert>

#if defined(__MINGW32__) || defined(__MINGW64__)
#include <direct.h>
#define MKDIR(A) _mkdir(A)
#else
#include <sys/stat.h>
#include <sys/types.h>
#define MKDIR(A) mkdir(A, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH)
#endif

using namespace std;

#if defined(__MINGW32__) || defined(__MINGW64__)
#define WINCODE(A) A
#define LICENSE    "UPPAAL/license.txt"
#define HOME       "HOMEPATH"
#else
#define WINCODE(A)
#define LICENSE ".uppaal/license.txt"
#define HOME    "HOME"
#endif

namespace io
{
    static size_t skip(const string& str, size_t i, bool comment)
    {
        while (i < str.length()) {
            switch (str[i]) {
            case ' ':
            case '\t':
            case '\r':
            case '\n': ++i; break;
            case '#':
                if (comment) {
                    while (++i < str.length() && str[i] != '\n')
                        ;
                    break;
                }
            default: return i;
            }
        }
        return i;
    }

    static size_t skipEnd(const string& str, size_t i)
    {
        while (i > 0) {
            switch (str[i - 1]) {
            case ' ':
            case '\t':
            case '\r':
            case '\n': --i; break;
            default: return i;
            }
        }
        return i;
    }

    static size_t trim(string& result)
    {
        size_t start = skip(result, 0, false);
        size_t end = skipEnd(result, result.length());
        if (start < end && end <= result.length()) {
            result = result.substr(start, end - start);
        } else {
            result = "";
        }
        return result.length();
    }

    static bool readLicense(string filename, string& lic)
    {
        bool ok = false;
        try {
            ifstream in(filename.c_str());
            if (in) {
                char c;
                stringstream ss;
                while (in.get(c))
                    ss << c;
                in.close();
                lic = ss.str();
                // cout << '<' << lic << '\n';
                ok = trim(lic) > 0;
            }
        } catch (...) {
        }
        return ok;
    }

    static void add_mac(const maclist_t* macs, size_t i, string* str, stringstream* data)
    {
        char buf[4];
        assert(i < macs->size);
        for (size_t j = 0; j < 6; ++j) {
            snprintf(buf, sizeof(buf), "%2.2x", macs->mac[i][j]);
            if (j > 0) {
                if (str) {
                    (*str) += ":";
                }
                if (data) {
                    (*data) << "%3A";
                }
            }
            if (str) {
                (*str) += buf;
            }
            if (data) {
                (*data) << buf;
            }
        }
    }

    static void get_macs(const maclist_t* macs, string* str, string& data, int defLease)
    {
        stringstream tmpdata;
        if (str) {
            (*str) = "";
        }
        tmpdata << "mac=";
        for (size_t i = 0; i < macs->size; ++i) {
            if (i > 0) {
                if (str) {
                    (*str) += ",";
                }
                tmpdata << "%2C";
            }
            add_mac(macs, i, str, &tmpdata);
        }
        if (macs->size > 0) {
            tmpdata << "&";
        }
        tmpdata << "lifetime=" << (defLease < 10 ? 30672000 : defLease);
        data = tmpdata.str();
    }

    const char* License::status2str(status_t s)
    {
        switch (s) {
        case VALID: return "$Valid_license";
        case EXPIRED: return "$License_has_expired";
        case EXHAUSTED: return "$Exhausted_license";
        case NOT_FOUND: return "$License_not_found";
        case CONNECTION_ERROR: return "$Connection_error_with_license_server";
        case NO_MAC: return "$MAC_address_not_found";
        case INVALID_MAC: return "$License_not_valid_on_this_machine";
        case INVALID:
        default: return "$Invalid_license";
        }
    }

    License::License(const char* keyStr, int defLease):
        key(keyStr ? keyStr : ""), defaultLease(defLease), status(INVALID), macs(base_getMAC())
    {
        tzset();
    }

    bool License::isValid(const char* toStr)
    {
        // Use only upper bound because of time skews/imprecisions.
        struct tm to;
        time_t now;
        time(&now);
        return strptime2(getValue(toStr).c_str(), "%a, %e %b %Y %H:%M:%S %z", &to, &now) &&
               difftime(mktime(&to), now) >= 0;
    }

    License::status_t License::checkStatus()
    {
        if (!macs || macs->size == 0) {
            // No MAC address => game over.
            status = NO_MAC;
            return status;
        }

        // Initialize mac & mac data.
        get_macs(macs, NULL, data, defaultLease);
        status = INVALID;

        // Read license file.
        string license;

        const char* home = getenv(HOME);
        if (!home) {
            cerr << "Your environment is missing the variable " HOME ".\n";
            abort();
        }
        string licName(home);
#if defined(__MINGW32__) || defined(__MINGW64__)
        licName += "/UPPAAL";
#else
        licName += "/.uppaal";  // Mac & Linux. Don't use Library/UPPAAL for simplicity.
#endif
        MKDIR(licName.c_str());
        licName += "/license.txt";

        bool ok = readLicense(licName, license);
        status = ok ? VALID : NOT_FOUND;
        if (ok) {
            decrypt(license);
            mapValues(license, '=');
            mapFeatures();
            if (key.length() == 0) {
                key = getValue("license.key");
            }
            status = VALID;
        } else if (key.length() == 0)  // Can't ask server.
        {
            status = NOT_FOUND;
        } else if (getLicense(key, license))  // Ask server.
        {
            decrypt(license);
            mapValues(license, '=');
            mapFeatures();
            status = VALID;
        }

        // Check validity.
        if (status == VALID && !isValid("lease.valid_to")) {
            status = EXPIRED;

            // We have key != license.key if it was given
            // in argument to update the license file or if
            // the license file was updated and the key is
            // the old one.
            // Try to renew with license.key.
            // If it does not work, try with key.

            if (getValue("license.floating") == "true" && isValid("license.expires_at")) {
                // Then renew the lease.
                string license;
                if (getLicense(getValue("license.key"), license)) {
                    decrypt(license);
                    mapValues(license, '=');
                    mapFeatures();
                    if (!isValid("lease.valid_to")) {
                        status = EXPIRED;
                    } else {
                        status = VALID;
                    }
                }
            }

            if (status != VALID && key.length() > 0 && key != getValue("license.key") &&
                getLicense(key, license)) {
                decrypt(license);
                mapValues(license, '=');
                mapFeatures();
                if (!isValid("lease.valid_to") || !isValid("license.expires_at")) {
                    status = EXPIRED;
                } else {
                    status = VALID;
                }
            }
        }

        // Check MAC.
        if (status == VALID) {
            string leaseMac = getValue("lease.mac");
            if (leaseMac != "") {
                bool ok = false;
                // Look for a matching MAC in the list.
                for (size_t i = 0; i < macs->size; ++i) {
                    string m;
                    add_mac(macs, i, &m, NULL);
                    if (leaseMac.find(m) != string::npos) {
                        ok = true;
                        break;
                    }
                }
                if (!ok) {
                    status = INVALID_MAC;
                }
            }
        }

        if (status != VALID) {
            values = std::map<std::string, std::string>();
        }

        return status;
    }

    bool License::getLicense(const string& key, string& result)
    {
        const char* str = NULL;
        stringstream post;
        post << "POST /lisa/licenses/" << key << " HTTP/1.0\n"
             << "Content-Type: application/x-www-form-urlencoded\n"
             << "Content-Length: " << data.length() << "\n\n"
             << data << "\n";

        Connection link("bugsy.grid.aau.dk", 80);
        if (!link.isOpen() || !link.write(post.str()) || (str = link.read()) == NULL) {
            status = CONNECTION_ERROR;
            return false;
        }

        result = str;
        mapValues(result, ':');

        string val = getValue("Status");
        if (val == "404 Not Found") {
            status = NOT_FOUND;
            return false;
        }
        if (val == "404 License exhausted") {
            status = EXHAUSTED;
            return false;
        }
        if (val == "403 License expired") {
            status = EXPIRED;
            return false;
        }
        if (val != "302 Found") {
            status = INVALID;
            return false;
        }

        val = getValue("Location");
        size_t i = val.find("http://");
        i = i < val.length() ? val.find("/", i + 7) : i;
        if (i >= val.length()) {
            status = CONNECTION_ERROR;
            return false;
        }
        string get("GET ");
        get += val.substr(i, val.length() - i);
        get += '\n';

        if (!link.reconnect() || !link.write(get) || (str = link.read()) == NULL ||
            trim(result = str) == 0) {
            status = CONNECTION_ERROR;
            return false;
        }

        try {
            // Save the license file.
            ofstream out(licFilename.c_str());
            (out << result).flush();
            out.close();
        } catch (...) {
        }

        status = VALID;
        return true;
    }

    void License::decrypt(string& str)
    {
        // Don't leave this stupid key in clear text anywhere.
        static const char stupidKey[] = {176, 138, 141, 223, 147, 150, 139, 139, 147, 154, 223,
                                         140, 154, 156, 141, 154, 139, 223, 138, 145, 139, 150,
                                         147, 223, 190, 147, 154, 135, 158, 145, 155, 141, 154,
                                         223, 155, 154, 147, 150, 137, 154, 141, 140, 223, 136,
                                         151, 158, 139, 223, 136, 154, 223, 145, 154, 154, 155};

        for (size_t i = 0; i < str.length();) {
            if (str[i] == '\n') {
                str.erase(i, 1);
            } else {
                ++i;
            }
        }
        try {
            str = Base64Coder::decode(str);
        } catch (...) {
            str = "";
        }
        for (size_t i = 0; i < str.length(); ++i) {
            str[i] = (char)(str[i] ^ ~stupidKey[i % sizeof(stupidKey)]);
        }
    }

    void License::mapValues(const string& str, char sep)
    {
        values = std::map<std::string, std::string>();
        for (size_t i = 0; i < str.length(); ++i) {
            size_t start = i = skip(str, i, true);
            bool eol = false;
            while (i < str.length() && str[i] != sep && !(eol = (str[i] == '\n'))) {
                ++i;
            }
            if (!eol && i < str.length()) {
                string k = str.substr(start, i - start);
                start = i = skip(str, i + 1, false);
                if (i >= str.length()) {
                    break;
                }
                while (i < str.length() && str[i] != '\n') {
                    ++i;
                }
                string v = str.substr(start, i - start);
                if (trim(k) > 0 && trim(v) > 0) {
                    values[k] = v;
                }
            }
        }
    }

    void License::mapFeatures()
    {
        features = std::map<std::string, bool>();

        string str = getValue("uppaal.features");
        for (size_t i = 0; i < str.length(); ++i) {
            size_t start = i = skip(str, i, false);
            if (start >= str.length()) {
                break;
            }
            while (i < str.length() && !(isspace(str[i]) || str[i] == ',')) {
                ++i;
            }
            features[str.substr(start, i - start)] = true;
        }
    }

}  // namespace io

#endif  // ENABLE_LICENSE
