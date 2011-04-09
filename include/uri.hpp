#ifndef YAPPI_URI_HPP
#define YAPPI_URI_HPP

#include <sstream>
#include <vector>
#include <map>

#include <uriparser/Uri.h>

// TODO: Make it beautiful

namespace yappi { namespace helpers {
    struct uri_t {
        uri_t(const std::string& source_):
            source(source_),
            port(0)
        {
            // URI parser initialization
            UriParserStateA state;
            UriUriA uri;

            state.uri = &uri;
           
            if(uriParseUriA(&state, source.c_str()) != URI_SUCCESS) {
                return;
            }
            
            // Scheme
            scheme = std::string(uri.scheme.first, uri.scheme.afterLast);
                
            // Userinfo
            userinfo = std::string(uri.userInfo.first, uri.userInfo.afterLast);
            
            // Host
            host = std::string(uri.hostText.first, uri.hostText.afterLast);
            
            // Port
            if(uri.portText.first && uri.portText.afterLast) {
                std::istringstream extract(std::string(
                    uri.portText.first, uri.portText.afterLast));
                extract >> port;
            }

            // Path
            UriPathSegmentA *segment = uri.pathHead;

            while(segment) {
                path.push_back(std::string(segment->text.first, segment->text.afterLast));
                segment = segment->next;
            }

            // Query parameters
            UriQueryListA *querylist = NULL;
            int count = 0;

            uriDissectQueryMallocA(&querylist, &count,
                uri.query.first, uri.query.afterLast);
           
            if(count) {
                while(querylist) {
                    query[querylist->key] = querylist->value;
                    querylist = querylist->next;
                }
            }

            uriFreeQueryListA(querylist);

            // Fragment
            fragment = std::string(uri.fragment.first, uri.fragment.afterLast);
            
            // Cleanup
            uriFreeUriMembersA(&uri);
        }

        std::string joinpath() {
            std::vector<std::string>::iterator it = path.begin();
            std::string result("/");

            while(true) {
                result += *it;
                ++it;

                if(it != path.end())
                    result += '/';
                else
                    break;
            }

            return result;
        }

        std::string source, scheme, userinfo, host, fragment;
        std::vector<std::string> path;
        std::map<std::string, std::string> query;
        unsigned int port;
    };
}}

#endif
