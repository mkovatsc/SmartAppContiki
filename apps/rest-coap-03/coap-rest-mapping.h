/*
 * coap-rest-mapping.h
 *
 *  Created on: 20.04.2011
 *      Author: Matthias Kovatsch
 */

#ifndef COAP_REST_MAPPING_H_
#define COAP_REST_MAPPING_H_

#define rest_get_method_type(request)                               coap_to_rest_method(coap_get_method(request))
#define rest_set_response_status(response, status)                  coap_set_code(response, status)

#define rest_get_header_content_type(request)                       coap_get_header_content_type(request)
#define rest_set_header_content_type(response, content_type)        coap_set_header_content_type(response, content_type)

#define rest_get_header_max_age(request, age)                       coap_get_header_max_age(request, age)
#define rest_set_header_max_age(response, age)                      coap_set_header_max_age(response, age)

#define rest_set_header_etag(response, etag, len)                   coap_set_header_etag(response, etag, len)

#define rest_get_header_host(request, host)                         coap_get_header_uri_host(request, host)

#define rest_set_header_location(response, uri)                     coap_set_header_location(response, uri)

#define rest_get_header_observe(request, observe)                   coap_get_header_observe(request, observe)
#define rest_set_header_observe(response, observe)                  coap_set_header_observe(response, observe)

#define rest_get_header_token(request, token)                       coap_get_header_token(request, token)
#define rest_set_header_token(response, token)                      coap_set_header_token(response, token)

#define rest_get_header_block(request, num, more, size, offset)     coap_get_header_block(request, num, more, size, offset)
#define rest_set_header_block(response, num, more, size)            coap_set_header_block(response, num, more, size)

#define rest_get_query(request, query)                              coap_get_header_uri_query(request, query)

#define rest_get_request_payload(request, payload)                  coap_get_payload(request, payload)
#define rest_set_response_payload(response, payload, size)          coap_set_payload(response, payload, size)

#define rest_get_query_variable(request, name, output, output_size) coap_get_query_variable(request, name, output, output_size)
#define rest_get_post_variable(request, name, output, output_size)  coap_get_post_variable(request, name, output, output_size)

#endif /* COAP_REST_MAPPING_H_ */
