#include "email.h"

namespace smtp
{
	
			mail::mail(const std::string& server, const std::string& user, const std::string& pwd): server_url{server}, username{user}, password{pwd}
			{
				curl = curl_easy_init();
			}
			
			mail::~mail()
			{
				curl_slist_free_all(recipients);
				curl_slist_free_all(headers);
				curl_easy_cleanup(curl);
				curl_mime_free(mime);
			}

			void mail::send() noexcept
			{
				
				if (!x_request_id.empty())
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

			void mail::add_attachment(const std::string& path, const std::string& filename, const std::string& encoding) noexcept
			{
				attachment doc {path, filename, encoding};
				documents.push_back(doc);
			}

			void mail::add_attachment(const std::string& path) noexcept
			{
				attachment doc {path, "", "base64"};
				documents.push_back(doc);
			}

			std::string mail::get_uuid() noexcept 
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

			std::string mail::get_response_date() noexcept
			{
				auto t = std::time(nullptr);
				auto tm = *std::localtime(&t);
				std::ostringstream oss;
				oss << std::put_time(&tm, "%a, %d %b %Y %H:%M:%S GMT");
				return oss.str();
			}	
	
}