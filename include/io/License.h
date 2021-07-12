// -*- mode: C++; c-file-style: "stroustrup"; c-basic-offset: 4; indent-tabs-mode: nil; -*-
////////////////////////////////////////////////////////////////////
//
// This file is a part of the UPPAAL toolkit.
// Copyright (c) 1995 - 2003, Uppsala University and Aalborg University.
// All right reserved.
//
///////////////////////////////////////////////////////////////////

#ifndef INCLUDE_IO_LICENSE_H
#define INCLUDE_IO_LICENSE_H
#ifdef ENABLE_LICENSE

#include "base/platform.h"
#include <string>
#include <map>
#include <vector>

namespace io
{
    class License
    {
    public:
        /** Initialize the license:
         * - use the key if necessary and if key != NULL,
         * - check the license in licfile,
         * The entries are mapped internally afterwards.
         */
        License(const char* key, int defLease = -1);

        enum status_t {
            VALID,      // Obviously valid license.
            EXPIRED,    // Time is over.
            EXHAUSTED,  // Too many other active users.
            NOT_FOUND,  // Key, server, or license file not found.
            INVALID,
            CONNECTION_ERROR,
            NO_MAC,
            INVALID_MAC
        };

        /** Conversion from status to string.
         */
        static const char* status2str(status_t);

        /** Check the license and return its status.
         * If it is expired, try to renew the license file
         * with its key if it is a floating license.
         */
        status_t checkStatus();

        /** Return the associated value to a key
         * inside the license file.
         */
        std::string getValue(const char* key) { return values[key]; }

        /** Return true if the string is listed in
         * uppaal.features, false otherwise.
         */
        bool hasFeature(const char* key) { return status == VALID && features[key]; }

        /** Iterator on the key/values.
         */
        typedef std::map<std::string, std::string>::const_iterator const_iterator;
        const_iterator begin() const { return values.begin(); }
        const_iterator end() const { return values.end(); }

        /** Iterator on the features.
         */
        typedef std::map<std::string, bool>::const_iterator feature_iterator;
        feature_iterator beginFeature() const { return features.begin(); }
        feature_iterator endFeature() const { return features.end(); }

    private:
        // Get the license from the server.
        bool getLicense(const std::string&, std::string&);

        // Decrypt the license file.
        void decrypt(std::string&);

        // Map the key/values in the license file.
        void mapValues(const std::string&, char);

        // Map the features.
        void mapFeatures();

        // Test (time) validity.
        bool isValid(const char*);

        std::string data, key;
        int defaultLease;
        status_t status;
        std::map<std::string, std::string> values;
        std::map<std::string, bool> features;
        maclist_t* macs;
    };
}  // namespace io

#endif  // ENABLE_LICENSE
#endif  // INCLUDE_IO_LICENSE_H
