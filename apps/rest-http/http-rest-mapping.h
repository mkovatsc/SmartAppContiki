/*
 * http-rest-mapping.h
 *
 *  Created on: 20.04.2011
 *      Author: Matthias Kovatsch
 */

#ifndef HTTP_REST_MAPPING_H_
#define HTTP_REST_MAPPING_H_

#define rest_get_method_type(request)                               (method_t)(request->request_type)
#define rest_set_response_status(response, status)                  http_set_status(response, status)

#define rest_get_header_content_type(request)                       http_get_header_content_type(request)
#define rest_set_header_content_type(response, content_type)        http_set_res_header(response, HTTP_HEADER_NAME_CONTENT_TYPE, http_get_content_type_string(content_type), 0)

#define rest_get_header_max_age(response, age)                      0
#define rest_set_header_max_age(response, age)                      http_set_res_header(response, HTTP_HEADER_NAME_CACHE_CONTROL, rest_to_http_max_age(age), 0)

#define rest_set_header_etag(response, etag, len)                   http_set_res_header(response, HTTP_HEADER_NAME_ETAG, rest_to_http_etag(etag, len), 1)

#define rest_get_header_host(request, host)                         http_get_header_host(request, host)

#define rest_set_header_location(response, uri)                     http_set_res_header(response, HTTP_HEADER_NAME_LOCATION, uri, 0)

#define rest_get_header_observe(request, observe)                   0
#define rest_set_header_observe(response, observe)                  0

#define rest_get_header_token(request, token)                       0
#define rest_set_header_token(response, token)                      0

#define rest_get_header_block(request, num, more, size)             0
#define rest_set_header_block(response, num, more, size)            0

#define rest_get_query(request, query)                              0

#define rest_get_request_payload(request, payload)                  http_get_req_payload(request, payload)
#define rest_set_response_payload(response, payload, size)          http_set_res_payload(response, payload, size)

#define rest_get_query_variable(request, name, output, output_size) http_get_query_variable(request, name, output, output_size)
#define rest_get_post_variable(request, name, output, output_size)  http_get_post_variable(request, name, output, output_size)

#endif /* HTTP_REST_MAPPING_H_ */
