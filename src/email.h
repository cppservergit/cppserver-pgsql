/*
 * email - send mail using libcurl (TLS-openssl)
 *
 *  Created on: June 6, 2023
 *      Author: Martín Córdova cppserver@martincordova.com - https://cppserver.com
 *      Disclaimer: some parts of this library may have been taken from sample code publicly available
 *		and written by third parties. Free to use in commercial projects, no warranties and no responsabilities assumed 
 *		by the author, use at your own risk. By using this code you accept the forementioned conditions.
 */
#ifndef EMAIL_H_
#define EMAIL_H_

#include <curl/curl.h>
#include <iostream>
#include <string>
#include <vector>
#include <random>

namespace smtp
{
	
	struct mail
	{
		public:
			std::string to;
			std::string cc;
			std::string subject;
			std::string body;
			bool debug_mode{false};
			std::string x_request_id{""};
	
			mail(const std::string& server, const std::string& user, const std::string& pwd): server_url{server}, username{user}, password{pwd}
			{
				curl = curl_easy_init();
			}
			
			~mail()
			{
				curl_slist_free_all(recipients);
				curl_slist_free_all(headers);
				curl_easy_cleanup(curl);
				curl_mime_free(mime);
			}
		
			void send() noexcept
			{
				
				logger::set_request_id(x_request_id);
				
				body.append("\r\n");
				
				std::vector<std::string> mail_headers {
					std::string("Date: " + get_response_date()),
					"To: " + to,
					"From: " + username,
					"Cc: " + cc,
					std::string("Message-ID: <" + get_uuid() + "@cppserver>"),
					"Subject: " + subject
				};

				if (curl) {
					if (debug_mode)
						curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
					curl_easy_setopt(curl, CURLOPT_USERNAME, username.c_str());
					curl_easy_setopt(curl, CURLOPT_PASSWORD, password.c_str());
					curl_easy_setopt(curl, CURLOPT_URL, server_url.c_str());
					if (server_url.ends_with(":587"))
						curl_easy_setopt(curl, CURLOPT_USE_SSL, (long)CURLUSESSL_ALL);
					curl_easy_setopt(curl, CURLOPT_MAIL_FROM, username.c_str());
					recipients = curl_slist_append(recipients, to.c_str());
					if (!cc.empty())
						recipients = curl_slist_append(recipients, cc.c_str());
					curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);
					
					for (const auto& h: mail_headers)
						headers = curl_slist_append(headers, h.c_str());
					curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

					mime = curl_mime_init(curl);

					part = curl_mime_addpart(mime);
					curl_mime_data(part, body.c_str(), CURL_ZERO_TERMINATED);
					curl_mime_type(part, "text/html");
					
					for (const auto& doc: documents)
					{
						part = curl_mime_addpart(mime);
						curl_mime_encoder(part, doc.encoding.c_str());
						curl_mime_filedata(part, doc.filesystem_path.c_str());
						if (!doc.filename.empty())
							curl_mime_filename(part, doc.filename.c_str());
					}
					
					logger::log("email", "info", "sending email to: " + to + " with subject: " + subject, true);
					
					curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);
					res = curl_easy_perform(curl);
				 
					if(res != CURLE_OK)
						logger::log("email", "error", std::string(__PRETTY_FUNCTION__) + " curl_easy_perform() failed: " + std::string(curl_easy_strerror(res)), true);
				}
			}

			void add_attachment(const std::string& path, const std::string& filename, const std::string& encoding = "base64" ) noexcept
			{
				attachment doc {path, filename, encoding};
				documents.push_back(doc);
			}

			void add_attachment(const std::string& path) noexcept
			{
				attachment doc {path, "", "base64"};
				documents.push_back(doc);
			}
			
		private:
			CURL *curl;
			CURLcode res = CURLE_OK;
			struct curl_slist *headers = NULL;
			struct curl_slist *recipients = NULL;
			curl_mime *mime;
			curl_mimepart *part;			
			std::string server_url;
			std::string username;
			std::string password;

			struct attachment
			{
				std::string filesystem_path;
				std::string filename;
				std::string encoding{"base64"};
			};
			std::vector<attachment> documents;

			std::string get_uuid() noexcept 
			{
				std::random_device dev;
				std::mt19937 rng(dev());
				std::uniform_int_distribution<int> dist(0, 15);

				const char *v = "0123456789abcdef";
				const bool dash[] = { 0, 0, 0, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0 };

				std::string res;
				for (int i = 0; i < 16; i++) {
					if (dash[i]) res += "-";
					res += v[dist(rng)];
					res += v[dist(rng)];
				}
				return res;
			}

			std::string get_response_date() noexcept
			{
				char buf[256];
				time_t now = time(0);
				struct tm tm = *gmtime(&now);
				strftime(buf, sizeof buf, "%a, %d %b %Y %H:%M:%S GMT", &tm);
				return std::string(buf);
			}		
	
	};
	
}

#endif /* EMAIL_H_ */
