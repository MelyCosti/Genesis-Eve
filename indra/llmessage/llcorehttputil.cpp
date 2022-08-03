/**
 * @file llcorehttputil.cpp
 * @date 2014-08-25
 * @brief Implementation of adapter and utility classes expanding the llcorehttp interfaces.
 *
 * $LicenseInfo:firstyear=2014&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2014, Linden Research, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#include "linden_common.h"

#include <algorithm>
#include <iterator>
#include <sstream>

// JsonCpp includes
#include "reader.h"
#include "writer.h"

#include "llcorehttputil.h"


#include "llhttpstatuscodes.h"
#include "llsdjson.h"
#include "llsdserialize.h"
#include "llevents.h"


using namespace LLCore;

namespace LLCoreHttpUtil
{

void logMessageSuccess(const std::string& log_auth, const std::string& url,
					   const std::string& message)
{
	llinfos << log_auth << ": Success '" << message << "' for: " << url
			<< llendl;
}

void logMessageFail(const std::string& log_auth, const std::string& url,
					const std::string& message)
{
	llwarns << log_auth << ": Failure '" << message << "' for: " << url
			<< llendl;
}


///////////////////////////////////////////////////////////////////////////////
// HttpRequestPumper class
///////////////////////////////////////////////////////////////////////////////

// The HttpRequestPumper is a utility class. When constructed it will poll the
// supplied HttpRequest once per frame until it is destroyed.
class HttpRequestPumper
{
public:
	HttpRequestPumper(const LLCore::HttpRequest::ptr_t& request);
	~HttpRequestPumper();

private:
	bool pollRequest();

private:
	LLBoundListener				mBoundListener;
	LLCore::HttpRequest::ptr_t	mHttpRequest;
};

HttpRequestPumper::HttpRequestPumper(const LLCore::HttpRequest::ptr_t& request)
:	mHttpRequest(request)
{
	LLEventPump& pump = LLEventPumps::instance().obtain("mainloop");
	mBoundListener = pump.listen(LLEventPump::ANONYMOUS,
								 boost::bind(&HttpRequestPumper::pollRequest,
											 this));
}

HttpRequestPumper::~HttpRequestPumper()
{
	if (mBoundListener.connected())
	{
		mBoundListener.disconnect();
	}
}

bool HttpRequestPumper::pollRequest()
{
	if (mHttpRequest && !mHttpRequest->isCancelled() &&
		mHttpRequest->getStatus() != gStatusCancelled)
	{
		mHttpRequest->update(0L);
	}
	// No point to keep pumping once the request is cancelled or destroyed...
	else if (mBoundListener.connected())
	{
		mBoundListener.disconnect();
	}
	return false;
}

///////////////////////////////////////////////////////////////////////////////
// *TODO: Currently converts only from XML content. A mode to convert using
// fromBinary() might be useful as well. Mesh headers could use it.
bool responseToLLSD(HttpResponse* response, bool log, LLSD& out_llsd)
{
	// Convert response to LLSD
	BufferArray* body = response ? response->getBody() : NULL;
	if (!body || !body->size())
	{
		return false;
	}

	LLCore::BufferArrayStream bas(body);
	LLSD body_llsd;
	S32 parse_status(LLSDSerialize::fromXML(body_llsd, bas, log));
	if (LLSDParser::PARSE_FAILURE == parse_status)
	{
		return false;
	}
	out_llsd = body_llsd;
	return true;
}

HttpHandle requestPostWithLLSD(HttpRequest* request,
							   HttpRequest::policy_t policy_id,
							   HttpRequest::priority_t priority,
							   const std::string& url, const LLSD& body,
							   const HttpOptions::ptr_t& options,
							   const HttpHeaders::ptr_t& headers,
							   const HttpHandler::ptr_t& handler)
{
	HttpHandle handle = LLCORE_HTTP_HANDLE_INVALID;
	if (!request) return handle;

	BufferArray* ba = new BufferArray();
	BufferArrayStream bas(ba);
	LLSDSerialize::toXML(body, bas);

	handle = request->requestPost(policy_id, priority, url, ba, options,
								  headers, handler);
	ba->release();
	return handle;
}

HttpHandle requestPutWithLLSD(HttpRequest* request,
							  HttpRequest::policy_t policy_id,
							  HttpRequest::priority_t priority,
							  const std::string& url, const LLSD& body,
							  const HttpOptions::ptr_t& options,
							  const HttpHeaders::ptr_t& headers,
							  const HttpHandler::ptr_t& handler)
{
	HttpHandle handle = LLCORE_HTTP_HANDLE_INVALID;
	if (!request) return handle;

	BufferArray* ba = new BufferArray();
	BufferArrayStream bas(ba);
	LLSDSerialize::toXML(body, bas);

	handle = request->requestPut(policy_id, priority, url, ba, options,
								 headers, handler);
	ba->release();
	return handle;
}

HttpHandle requestPatchWithLLSD(HttpRequest* request,
								HttpRequest::policy_t policy_id,
								HttpRequest::priority_t priority,
								const std::string& url, const LLSD& body,
								const HttpOptions::ptr_t& options,
								const HttpHeaders::ptr_t& headers,
								const HttpHandler::ptr_t& handler)
{
	HttpHandle handle = LLCORE_HTTP_HANDLE_INVALID;
	if (!request) return handle;

	BufferArray* ba = new BufferArray();
	BufferArrayStream bas(ba);
	LLSDSerialize::toXML(body, bas);

	handle = request->requestPatch(policy_id, priority, url, ba, options,
								   headers, handler);
	ba->release();
	return handle;
}

std::string responseToString(LLCore::HttpResponse* response)
{
	static const std::string empty = "[Empty]";

	if (!response)
	{
		return empty;
	}

	BufferArray* body = response->getBody();
	if (!body || !body->size())
	{
		return empty;
	}

	// Attempt to parse as LLSD regardless of content-type
	LLSD body_llsd;
	if (responseToLLSD(response, false, body_llsd))
	{
		std::ostringstream tmp;
		LLSDSerialize::toPrettyNotation(body_llsd, tmp);
		size_t temp_len = tmp.tellp();
		if (temp_len)
		{
			return tmp.str().substr(0, llmin(temp_len, (size_t)1024));
		}
	}
	else
	{
		// *TODO: More elaborate forms based on Content-Type as needed.
		char content[1024];
		size_t len = body->read(0, content, sizeof(content));
		if (len)
		{
			return std::string(content, 0, len);
		}
	}

	// Default
	return empty;
}

///////////////////////////////////////////////////////////////////////////////
// HttpCoroHandler class
///////////////////////////////////////////////////////////////////////////////

HttpCoroHandler::HttpCoroHandler(LLEventStream& reply)
:	mReplyPump(reply)
{
}

void HttpCoroHandler::onCompleted(LLCore::HttpHandle handle,
								  LLCore::HttpResponse* response)
{
	if (!response)
	{
		llwarns << "NULL response pointer passed !" << llendl;
		return;
	}

	static const LLCore::HttpStatus bad_handle(LLCore::HttpStatus::LLCORE,
											   LLCore::HE_HANDLE_NOT_FOUND);
	LLCore::HttpStatus status = response->getStatus();
	if (status == bad_handle)
	{
		// A response came in for a cancelled request and we have not processed
		// the cancel yet. Patience !
		return;
	}

	LLSD result;
	if (!status)
	{
		bool parse_success = false;
		result = LLSD::emptyMap();
		LLCore::HttpStatus::type_enum_t error = status.getType();

		LL_DEBUGS("CoreHttp") << "Error " << status.toTerseString()
							  << " - Cannot access url: "
							  << response->getRequestURL()
							  << " - Reason: " << status.toString() << LL_ENDL;
		if (error >= 400 && error < 500)
		{
			LLSD body = parseBody(response, parse_success);
			if (!body.isUndefined())
			{
				if (!body.isMap())
				{
					result[HttpCoroutineAdapter::HTTP_RESULTS_CONTENT] = body;
				}
				else
				{
					result = body;
				}
			}
		}
	}
	else
	{
		result = handleSuccess(response, status);
	}

	buildStatusEntry(response, status, result);

	if (!status)
	{
		LLSD& status_llsd = result[HttpCoroutineAdapter::HTTP_RESULTS];
		bool parse_success = false;
		LLSD error_body = parseBody(response, parse_success);
		if (parse_success)
		{
			status_llsd["error_body"] = error_body;
			LL_DEBUGS("CoreHttp") << "Returned body:\n";
			std::stringstream str;
			LLSDSerialize::toPrettyXML(error_body, str);
			LL_CONT << str.str() << LL_ENDL;
		}
		else
		{
			LLCore::BufferArray* body = response->getBody();
			LLCore::BufferArrayStream bas(body);
			LLSD::String bodydata;
			bodydata.reserve(response->getBodySize());
			bas >> std::noskipws;
			bodydata.assign(std::istream_iterator<U8>(bas),
							std::istream_iterator<U8>());
			status_llsd["error_body"] = LLSD(bodydata);
			LL_DEBUGS("CoreHttp") << "Returned body:" << std::endl
								  << status_llsd["error_body"].asString()
								  << LL_ENDL;
		}
	}

	mReplyPump.post(result);
}

void HttpCoroHandler::buildStatusEntry(LLCore::HttpResponse* response,
									   LLCore::HttpStatus status, LLSD& result)
{
	if (!response)
	{
		llwarns << "NULL response pointer passed !" << llendl;
		return;
	}

	LLSD httpresults = LLSD::emptyMap();

	writeStatusCodes(status, response->getRequestURL(), httpresults);

	LLSD httpheaders = LLSD::emptyMap();

	LLCore::HttpHeaders::ptr_t hdrs = response->getHeaders();
	if (hdrs)
	{
		for (LLCore::HttpHeaders::iterator it = hdrs->begin(),
										   end = hdrs->end();
			 it != end; ++it)
		{
			if (!it->second.empty())
			{
				httpheaders[it->first] = it->second;
			}
			else
			{
				httpheaders[it->first] = LLSD::Boolean(true);
			}
		}
	}

	httpresults[HttpCoroutineAdapter::HTTP_RESULTS_HEADERS] = httpheaders;
	result[HttpCoroutineAdapter::HTTP_RESULTS] = httpresults;
}

void HttpCoroHandler::writeStatusCodes(LLCore::HttpStatus status,
									   const std::string& url, LLSD& result)
{
	result[HttpCoroutineAdapter::HTTP_RESULTS_SUCCESS] = LLSD::Boolean(status);
	result[HttpCoroutineAdapter::HTTP_RESULTS_TYPE] = LLSD::Integer(status.getType());
	result[HttpCoroutineAdapter::HTTP_RESULTS_STATUS] = LLSD::Integer(status.getStatus());
	result[HttpCoroutineAdapter::HTTP_RESULTS_MESSAGE] = LLSD::String(status.getMessage());
	result[HttpCoroutineAdapter::HTTP_RESULTS_URL] = LLSD::String(url);
}



///////////////////////////////////////////////////////////////////////////////
// HttpCoroRawHandler class
///////////////////////////////////////////////////////////////////////////////

// HttpCoroRawHandler is a specialization of the LLCore::HttpHandler for
// interacting with coroutines.
//
// In addition to the normal "http_results" the returned LLSD will contain an
// entry keyed with "raw" containing the unprocessed results of the HTTP call.

class HttpCoroRawHandler final : public HttpCoroHandler
{
public:
	HttpCoroRawHandler(LLEventStream& reply)
	:	HttpCoroHandler(reply)
	{
	}

	LLSD handleSuccess(LLCore::HttpResponse* response,
					   LLCore::HttpStatus& status) override;
	LLSD parseBody(LLCore::HttpResponse* response, bool& success) override;
};

LLSD HttpCoroRawHandler::handleSuccess(LLCore::HttpResponse* response,
									   LLCore::HttpStatus& status)
{
	LLSD result = LLSD::emptyMap();

	BufferArray* body = response ? response->getBody() : NULL;
	if (!body || !body->size())
	{
		return result;
	}

	size_t size = body->size();

	LLCore::BufferArrayStream bas(body);

	LLSD::Binary data;
	data.reserve(size);
	bas >> std::noskipws;
	data.assign(std::istream_iterator<U8>(bas), std::istream_iterator<U8>());

	result[HttpCoroutineAdapter::HTTP_RESULTS_RAW] = data;

	return result;
}

LLSD HttpCoroRawHandler::parseBody(LLCore::HttpResponse* response,
								   bool& success)
{
	success = true;
	return LLSD();
}

///////////////////////////////////////////////////////////////////////////////
// HttpCoroJSONHandler class
///////////////////////////////////////////////////////////////////////////////

// HttpCoroJSONHandler is a specialization of the LLCore::HttpHandler for
// interacting with coroutines.
//
// In addition to the normal "http_results" the returned LLSD will contain JSON
// entries will be converted into an LLSD map. All results are considered
// strings

class HttpCoroJSONHandler final : public HttpCoroHandler
{
public:
	HttpCoroJSONHandler(LLEventStream& reply)
	:	HttpCoroHandler(reply)
	{
	}

	LLSD handleSuccess(LLCore::HttpResponse* response,
					   LLCore::HttpStatus& status) override;
	LLSD parseBody(LLCore::HttpResponse* response, bool& success) override;
};

LLSD HttpCoroJSONHandler::handleSuccess(LLCore::HttpResponse* response,
										LLCore::HttpStatus& status)
{
	LLSD result = LLSD::emptyMap();

	//genesis comment
	LL_INFOS() << "not implemented method HttpCoroJSONHandler::handleSuccess " << LL_ENDL;

	return result;
}

LLSD HttpCoroJSONHandler::parseBody(LLCore::HttpResponse* response,
									bool& success)
{
	//genesis comment
	LL_INFOS() << "not implemented method HttpCoroJSONHandler::parseBody " << LL_ENDL;
	return LLSD::emptyMap();
}

///////////////////////////////////////////////////////////////////////////////
// HttpCoroutineAdapter class
///////////////////////////////////////////////////////////////////////////////

const std::string HttpCoroutineAdapter::HTTP_RESULTS("http_result");
const std::string HttpCoroutineAdapter::HTTP_RESULTS_SUCCESS("success");
const std::string HttpCoroutineAdapter::HTTP_RESULTS_TYPE("type");
const std::string HttpCoroutineAdapter::HTTP_RESULTS_STATUS("status");
const std::string HttpCoroutineAdapter::HTTP_RESULTS_MESSAGE("message");
const std::string HttpCoroutineAdapter::HTTP_RESULTS_URL("url");
const std::string HttpCoroutineAdapter::HTTP_RESULTS_HEADERS("headers");
const std::string HttpCoroutineAdapter::HTTP_RESULTS_CONTENT("content");
const std::string HttpCoroutineAdapter::HTTP_RESULTS_RAW("raw");

//static
HttpCoroutineAdapter::instances_list_t HttpCoroutineAdapter::sInstances;

HttpCoroutineAdapter::HttpCoroutineAdapter(const std::string& name,
										   LLCore::HttpRequest::ptr_t& request,
										   LLCore::HttpRequest::policy_t polid,
										   LLCore::HttpRequest::priority_t pri)
:	mAdapterName(name),
	mPolicyId(polid),
	mPriority(pri),
	mYieldingHandle(LLCORE_HTTP_HANDLE_INVALID),
	mRequest(request)
{
	sInstances.insert(this);
}

HttpCoroutineAdapter::HttpCoroutineAdapter(const std::string& name,
										   LLCore::HttpRequest::policy_t polid,
										   LLCore::HttpRequest::priority_t pri)
:	mAdapterName(name),
	mPolicyId(polid),
	mPriority(pri),
	mYieldingHandle(LLCORE_HTTP_HANDLE_INVALID),
	mRequest(DEFAULT_HTTP_REQUEST)
{
	sInstances.insert(this);
}

HttpCoroutineAdapter::~HttpCoroutineAdapter()
{
	sInstances.erase(this);
	cancelSuspendedOperation();
}

//static
void HttpCoroutineAdapter::cleanup()
{
	if (sInstances.empty())
	{
		return;
	}
	llinfos << "Cancelling suspended operations on remaining adapters..."
			<< llendl;
	// Note: just in case calling cancelSuspendedOperation() on one adapter
	// instance would destroy another instance, we consider that sInstances
	// could get modified during the loop, and thus copy it beforehand and
	// check each time that the adapter still exists in sInstances before
	// attempting to cancel its operation...
	instances_list_t copy = sInstances;
	for (instances_list_t::const_iterator it = copy.begin(), end = copy.end();
		 it != end; ++it)
	{
		HttpCoroutineAdapter* adapter = *it;
		if (sInstances.count(adapter))
		{
			adapter->cancelSuspendedOperation();
		}
	}
}

LLSD HttpCoroutineAdapter::postAndSuspend(const std::string& url,
										  const LLSD& body,
										  LLCore::HttpOptions::ptr_t options,
										  LLCore::HttpHeaders::ptr_t headers)
{
	if (!mRequest)
	{
		llwarns << "Invalid request !" << llendl;
		return LLSD();
	}

	LLEventStream reply_pump(mAdapterName, true);
	//genesis comment
	LL_INFOS() << "I am in an umplementid method HttpCoroutineAdapter::postAndSuspend" <<LL_ENDL;
	//TODO?
	LLSD result;
	return result;
	//genesis comment
}

LLSD HttpCoroutineAdapter::postAndSuspend_(const std::string& url,
										   const LLSD& body,
										   LLCore::HttpOptions::ptr_t& options,
										   LLCore::HttpHeaders::ptr_t& headers,
										   HttpCoroHandler::ptr_t& handler)
{
	LL_INFOS () << " I am in an unimplemented method : HttpCoroutineAdapter::postAndSuspend_" <<LL_ENDL;
	//genesis comment
	//TODO?
	LLSD results;
	return results;
}

LLSD HttpCoroutineAdapter::postAndSuspend(const std::string& url,
										  LLCore::BufferArray::ptr_t rawbody,
										  LLCore::HttpOptions::ptr_t options,
										  LLCore::HttpHeaders::ptr_t headers)
{
	if (!mRequest)
	{
		llwarns << "Invalid request !" << llendl;
		return LLSD();
	}

	LLEventStream reply_pump(mAdapterName, true);
	//genesis comment
	LL_INFOS() << "I am in an umplementid method HttpCoroutineAdapter::postAndSuspend" <<LL_ENDL;
	//TODO?
	LLSD result;
	return result;
	//genesis comment
}

LLSD HttpCoroutineAdapter::postRawAndSuspend(const std::string& url,
											 LLCore::BufferArray::ptr_t rawbody,
											 LLCore::HttpOptions::ptr_t options,
											 LLCore::HttpHeaders::ptr_t headers)
{
	if (!mRequest)
	{
		llwarns << "Invalid request !" << llendl;
		return LLSD();
	}

	LLEventStream reply_pump(mAdapterName, true);
	HttpCoroHandler::ptr_t http_handler =
		std::make_shared<HttpCoroRawHandler>(reply_pump);

	return postAndSuspend_(url, rawbody, options, headers, http_handler);
}

// *TODO: This functionality could be moved into the LLCore::Http library
// itself by having the CURL layer read the file directly.
LLSD HttpCoroutineAdapter::postFileAndSuspend(const std::string& url,
											  std::string filename,
											  LLCore::HttpOptions::ptr_t options,
											  LLCore::HttpHeaders::ptr_t headers)
{
	if (!mRequest)
	{
		llwarns << "Invalid request !" << llendl;
		return LLSD();
	}

	LLCore::BufferArray::ptr_t filedata(new LLCore::BufferArray);

	// Scoping for our streams so that they go away when we no longer need them
	{
		LLCore::BufferArrayStream outs(filedata.get());
		llifstream ins(filename.c_str(),
					   std::iostream::binary | std::iostream::out);
		if (ins.is_open())
		{
			ins.seekg(0, std::ios::beg);
			ins >> std::noskipws;

			std::copy(std::istream_iterator<U8>(ins),
					  std::istream_iterator<U8>(),
					  std::ostream_iterator<U8>(outs));

			ins.close();
		}
	}

	return postAndSuspend(url, filedata, options, headers);
}

// *TODO: This functionality could be moved into the LLCore::Http library
// itself by having the CURL layer read the file directly.
LLSD HttpCoroutineAdapter::postFileAndSuspend(const std::string& url,
											  const LLUUID& assetid,
											  LLAssetType::EType assetType,
											  LLCore::HttpOptions::ptr_t options,
											  LLCore::HttpHeaders::ptr_t headers)
{
	LL_INFOS () << " I am in an unimplemented method : HttpCoroutineAdapter::postFileAndSuspend" <<LL_ENDL;
	//genesis comment
	//TODO?
	LLSD result;
	return result;
}

LLSD HttpCoroutineAdapter::postJsonAndSuspend(const std::string& url,
											  const LLSD& body,
											  LLCore::HttpOptions::ptr_t options,
											  LLCore::HttpHeaders::ptr_t headers)
{
	LL_INFOS () << " I am in an unimplemented method : HttpCoroutineAdapter::postJsonAndSuspend" <<LL_ENDL;
	//genesis comment
	//TODO?
	LLSD result;
	return result;
}

LLSD HttpCoroutineAdapter::postAndSuspend_(const std::string& url,
										   LLCore::BufferArray::ptr_t& rawbody,
										   LLCore::HttpOptions::ptr_t& options,
										   LLCore::HttpHeaders::ptr_t& headers,
										   HttpCoroHandler::ptr_t& handler)
{
	LL_INFOS () << " I am in an unimplemented method : HttpCoroutineAdapter::postAndSuspend_" <<LL_ENDL;
	//genesis comment
	//TODO?
	LLSD result;
	return result;
}



LLSD HttpCoroutineAdapter::putAndSuspend(const std::string& url,
										 const LLSD& body,
										 LLCore::HttpOptions::ptr_t options,
										 LLCore::HttpHeaders::ptr_t headers)
{
	if (!mRequest)
	{
		llwarns << "Invalid request !" << llendl;
		return LLSD();
	}

	LLEventStream reply_pump(mAdapterName + "Reply", true);
	//genesis comment
	LL_INFOS() << "I am in an umplementid method HttpCoroutineAdapter::putAndSuspend" <<LL_ENDL;
	//TODO?
	LLSD result;
	return result;
	//genesis comment
}

LLSD HttpCoroutineAdapter::putJsonAndSuspend(const std::string& url,
										 	 const LLSD& body,
										 	 LLCore::HttpOptions::ptr_t options,
										 	 LLCore::HttpHeaders::ptr_t headers)
{
	if (!mRequest)
	{
		llwarns << "Invalid request !" << llendl;
		return LLSD();
	}

	LLEventStream reply_pump(mAdapterName, true);
	//genesis comment
	LL_INFOS() << "I am in an umplementid method HttpCoroutineAdapter::putJsonAndSuspend" <<LL_ENDL;
	//TODO?
	LLSD result;
	return result;
	//genesis comment
}

LLSD HttpCoroutineAdapter::putAndSuspend_(const std::string& url,
										  const LLSD& body,
										  LLCore::HttpOptions::ptr_t& options,
										  LLCore::HttpHeaders::ptr_t& headers,
										  HttpCoroHandler::ptr_t& handler)
{
		LL_INFOS () << " I am in an unimplemented method : HttpCoroutineAdapter::putAndSuspend_" <<LL_ENDL;
	//genesis comment
	//TODO?
	LLSD result;
	return result;
}

LLSD HttpCoroutineAdapter::putAndSuspend_(const std::string& url,
										  const LLCore::BufferArray::ptr_t& rawbody,
										  LLCore::HttpOptions::ptr_t& options,
										  LLCore::HttpHeaders::ptr_t& headers,
										  HttpCoroHandler::ptr_t& handler)
{
	LL_INFOS () << " I am in an unimplemented method : HttpCoroutineAdapter::putAndSuspend_" <<LL_ENDL;
	//genesis comment
	//TODO?
	LLSD result;
	return result;
}

LLSD HttpCoroutineAdapter::getAndSuspend(const std::string& url,
										 LLCore::HttpOptions::ptr_t options,
										 LLCore::HttpHeaders::ptr_t headers)
{
	if (!mRequest)
	{
		llwarns << "Invalid request !" << llendl;
		return LLSD();
	}
	//genesis comment
	//LLEventStream reply_pump(mAdapterName + "Reply", true);
	//LLEventStream reply_pump(mAdapterName, true);
	
	LL_INFOS() << "I am in an umplementid method HttpCoroutineAdapter::getAndSuspend" <<LL_ENDL;
	//TODO?
	LLSD result;
	return result;
	//genesis comment
}

LLSD HttpCoroutineAdapter::getRawAndSuspend(const std::string& url,
											LLCore::HttpOptions::ptr_t options,
											LLCore::HttpHeaders::ptr_t headers)
{
	if (!mRequest)
	{
		llwarns << "Invalid request !" << llendl;
		return LLSD();
	}

	LLEventStream reply_pump(mAdapterName + "Reply", true);
	//genesis comment
	LL_INFOS() << "I am in an umplementid method HttpCoroutineAdapter::getRawAndSuspend" <<LL_ENDL;
	//TODO?
	LLSD result;
	return result;
	//genesis comment
}

LLSD HttpCoroutineAdapter::getJsonAndSuspend(const std::string& url,
											 LLCore::HttpOptions::ptr_t options,
											 LLCore::HttpHeaders::ptr_t headers)
{
	if (!mRequest)
	{
		llwarns << "Invalid request !" << llendl;
		return LLSD();
	}

	LLEventStream reply_pump(mAdapterName + "Reply", true);
	//genesis comment
	LL_INFOS() << "I am in an umplementid method HttpCoroutineAdapter::getJsonAndSuspend" <<LL_ENDL;
	//TODO?
	LLSD result;
	return result;
	//genesis comment
}

LLSD HttpCoroutineAdapter::getAndSuspend_(const std::string& url,
										  LLCore::HttpOptions::ptr_t& options,
										  LLCore::HttpHeaders::ptr_t& headers,
										  HttpCoroHandler::ptr_t& handler)
{
	LL_INFOS () << " I am in an unimplemented method : HttpCoroutineAdapter::getAndSuspend_" <<LL_ENDL;
	//genesis comment
	//TODO?
	LLSD result;
	return result;
}

LLSD HttpCoroutineAdapter::deleteAndSuspend(const std::string& url,
											LLCore::HttpOptions::ptr_t options,
											LLCore::HttpHeaders::ptr_t headers)
{
	if (!mRequest)
	{
		llwarns << "Invalid request !" << llendl;
		return LLSD();
	}

	LLEventStream reply_pump(mAdapterName + "Reply", true);
	//genesis comment
	LL_INFOS() << "I am in an umplementid method HttpCoroutineAdapter::deleteAndSuspend" <<LL_ENDL;
	//TODO?
	LLSD result;
	return result;
	//genesis comment
}

LLSD HttpCoroutineAdapter::deleteJsonAndSuspend(const std::string& url,
												LLCore::HttpOptions::ptr_t options,
												LLCore::HttpHeaders::ptr_t headers)
{
	if (!mRequest)
	{
		llwarns << "Invalid request !" << llendl;
		return LLSD();
	}

	LLEventStream reply_pump(mAdapterName + "Reply", true);
	//genesis comment
	LL_INFOS() << "I am in an umplementid method HttpCoroutineAdapter::deleteJsonAndSuspend" <<LL_ENDL;
	//TODO?
	LLSD result;
	return result;
	//genesis comment
}

LLSD HttpCoroutineAdapter::deleteAndSuspend_(const std::string& url,
											 LLCore::HttpOptions::ptr_t& options,
											 LLCore::HttpHeaders::ptr_t& headers,
											 HttpCoroHandler::ptr_t& handler)
{
	//genesis comment
	LL_INFOS() << "I am in an umplementid method HttpCoroutineAdapter::deleteAndSuspend_" <<LL_ENDL;
	//TODO?
	LLSD result;
	return result;
	//genesis comment
}

LLSD HttpCoroutineAdapter::patchAndSuspend(const std::string& url,
										   const LLSD& body,
										   LLCore::HttpOptions::ptr_t options,
										   LLCore::HttpHeaders::ptr_t headers)
{
	if (!mRequest)
	{
		llwarns << "Invalid request !" << llendl;
		return LLSD();
	}

	LLEventStream reply_pump(mAdapterName + "Reply", true);
	//genesis comment
	LL_INFOS() << "I am in an umplementid method HttpCoroutineAdapter::patchAndSuspend" <<LL_ENDL;
	//TODO?
	LLSD result;
	return result;
	//genesis comment
}

LLSD HttpCoroutineAdapter::patchAndSuspend_(const std::string& url,
										    const LLSD& body,
										    LLCore::HttpOptions::ptr_t& options,
										    LLCore::HttpHeaders::ptr_t& headers,
										    HttpCoroHandler::ptr_t& handler)
{
	LL_INFOS () << " I am in an unimplemented method : HttpCoroutineAdapter::patchAndSuspend_" <<LL_ENDL;
	//genesis comment
	//TODO?
	LLSD result;
	return result;
}

LLSD HttpCoroutineAdapter::copyAndSuspend(const std::string& url,
										  const std::string dest,
										  LLCore::HttpOptions::ptr_t options,
										  LLCore::HttpHeaders::ptr_t headers)
{
	if (!mRequest)
	{
		llwarns << "Invalid request !" << llendl;
		return LLSD();
	}

	LLEventStream reply_pump(mAdapterName + "Reply", true);
	//genesis comment
	LL_INFOS() << "I am in an umplementid method HttpCoroutineAdapter::copyAndSuspend" <<LL_ENDL;
	//TODO?
	LLSD result;
	return result;
	//genesis comment
}

LLSD HttpCoroutineAdapter::copyAndSuspend_(const std::string& url,
										   LLCore::HttpOptions::ptr_t& options,
										   LLCore::HttpHeaders::ptr_t& headers,
										   HttpCoroHandler::ptr_t& handler)
{
	LL_INFOS () << " I am in an unimplemented method : HttpCoroutineAdapter::copyAndSuspend_" <<LL_ENDL;
	//genesis comment
	//TODO?
	LLSD result;
	return result;
}

LLSD HttpCoroutineAdapter::moveAndSuspend(const std::string& url,
										  const std::string dest,
										  LLCore::HttpOptions::ptr_t options,
										  LLCore::HttpHeaders::ptr_t headers)
{
	if (!mRequest)
	{
		llwarns << "Invalid request !" << llendl;
		return LLSD();
	}

	LLEventStream reply_pump(mAdapterName + "Reply", true);
	//genesis comment
	LL_INFOS() << "I am in an umplementid method HttpCoroutineAdapter::moveAndSuspend" <<LL_ENDL;
	//TODO?
	LLSD result;
	return result;
	//genesis comment
}

LLSD HttpCoroutineAdapter::moveAndSuspend_(const std::string& url,
										   LLCore::HttpOptions::ptr_t& options,
										   LLCore::HttpHeaders::ptr_t& headers,
										   HttpCoroHandler::ptr_t& handler)
{
	LL_INFOS () << " I am in an unimplemented method : HttpCoroutineAdapter::copyAndSuspend_" <<LL_ENDL;
	//genesis comment
	//TODO?
	LLSD result;
	return result;
}

void HttpCoroutineAdapter::checkDefaultHeaders(LLCore::HttpHeaders::ptr_t& headers)
{
	if (!headers)
	{
		headers.reset(new LLCore::HttpHeaders);
	}
	if (!headers->find(HTTP_OUT_HEADER_ACCEPT))
	{
		headers->append(HTTP_OUT_HEADER_ACCEPT, HTTP_CONTENT_LLSD_XML);
	}
	if (!headers->find(HTTP_OUT_HEADER_CONTENT_TYPE))
	{
		headers->append(HTTP_OUT_HEADER_CONTENT_TYPE, HTTP_CONTENT_LLSD_XML);
	}
	//genesis comment
	//if (gMessageSystemp && !headers->find("X-SecondLife-UDP-Listen-Port"))
	//{
	//	headers->append("X-SecondLife-UDP-Listen-Port",
	//					llformat("%d", gMessageSystemp->mPort));
	//}
	//genesis comment
}

void HttpCoroutineAdapter::cancelSuspendedOperation()
{
	LLCore::HttpRequest::ptr_t request = mWeakRequest.lock();
	HttpCoroHandler::ptr_t handler = mWeakHandler.lock();
	if (request && handler && mYieldingHandle != LLCORE_HTTP_HANDLE_INVALID)
	{
		llinfos << "Cancelling yielding request for " << mAdapterName
				<< llendl;
		request->requestCancel(mYieldingHandle, handler);
		cleanState();
		// We now need to wake up the coroutine, else it might keep sitting
		// there for a while, which would be a problem on viewer exit. HB
		// First, create a fake result LLSD with cancelled operation as the
		// error status.
		LLSD status = LLSD::emptyMap();
		HttpCoroHandler::writeStatusCodes(gStatusCancelled, mURL, status);
		LLSD result = LLSD::emptyMap();
		result["http_result"] = status;
		// Then, post to the associated pump, passing the fake result LLSD.
		// IMPORTANT: since this call shall result in the exit of the coroutine
		// and likely the destruction of our adapter instance, it *must* be
		// immediately followed by a return.
		handler->getReplyPump().post(result);
		return;
	}

	LL_DEBUGS("CoreHttp") << "Operation for " << mAdapterName;
	if (!handler)
	{
		LL_CONT << " already finished";
	}
	else if (!request)
	{
		LL_CONT << " got a NULL request pointer";
	}
	else
	{
		LL_CONT << " not yielding";
	}
	LL_CONT << LL_ENDL;
}

void HttpCoroutineAdapter::saveState(const std::string& url,
								     LLCore::HttpHandle yielding_handle,
									 HttpCoroHandler::ptr_t& handler)
{
	mURL = url;
	mWeakRequest = mRequest;
	mWeakHandler = handler;
	mYieldingHandle = yielding_handle;
}

void HttpCoroutineAdapter::cleanState()
{
	mURL.clear();
	mWeakRequest.reset();
	mWeakHandler.reset();
	mYieldingHandle = LLCORE_HTTP_HANDLE_INVALID;
}

LLSD HttpCoroutineAdapter::buildImmediateErrorResult(const std::string& url)
{
	if (!mRequest)
	{
		llwarns << "Invalid request !" << llendl;
		return LLSD();
	}

	LLCore::HttpStatus status = mRequest->getStatus();
	llwarns << "Error posting to: " << url << " - Status: "
			<< status.getStatus() << " - Message: " << status.getMessage()
			<< llendl;

	// Mimic the status results returned from an http error that we had to wait
	// on
	LLSD httpresults = LLSD::emptyMap();

	HttpCoroHandler::writeStatusCodes(status, url, httpresults);

	LLSD errors = LLSD::emptyMap();
	errors["http_result"] = httpresults;

	return errors;
}

//static
LLCore::HttpStatus HttpCoroutineAdapter::getStatusFromLLSD(const LLSD& results)
{
	const LLSD* resp;
	if (results.has(HttpCoroutineAdapter::HTTP_RESULTS))
	{
		resp = &results[HttpCoroutineAdapter::HTTP_RESULTS];
	}
	else
	{
		resp = &results;
	}

	S32 type = (*resp)[HttpCoroutineAdapter::HTTP_RESULTS_TYPE].asInteger();
	S32 code = (*resp)[HttpCoroutineAdapter::HTTP_RESULTS_STATUS].asInteger();

	return LLCore::HttpStatus((LLCore::HttpStatus::type_enum_t)type,
							  (short)code);
}

//static
void HttpCoroutineAdapter::callbackHttpGet(const std::string& url,
										   LLCore::HttpRequest::policy_t policy_id,
										   completionCallback_t success,
										   completionCallback_t failure)
{
	LLCoros::instance().launch("HttpCoroutineAdapter::genericGetCoro",
				  boost::bind(&HttpCoroutineAdapter::trivialGetCoro, url,
							  policy_id, success, failure));
}

//static
void HttpCoroutineAdapter::messageHttpGet(const std::string& url,
										  const std::string& success,
										  const std::string& failure)
{
	completionCallback_t cb_succ = NULL;
	//genesis comment
	//if (!success.empty())
	//{
	//	cb_succ = (completionCallback_t)boost::bind(&logMessageSuccess,
	//												"HttpCoroutineAdapter",
	//												url, success);
	//}
	//genesis comment
	completionCallback_t cb_fail = NULL;
	if (!failure.empty())
	{
		cb_fail = (completionCallback_t)boost::bind(&logMessageFail,
													"HttpCoroutineAdapter",
													url, failure);
	}

	callbackHttpGet(url, cb_succ, cb_fail);
}

//static
void HttpCoroutineAdapter::trivialGetCoro(std::string url,
										  LLCore::HttpRequest::policy_t policy_id,
										  completionCallback_t success,
										  completionCallback_t failure)
{
	LL_DEBUGS("CoreHttp") << "Generic GET for: " << url << LL_ENDL;

	LLCore::HttpOptions::ptr_t options(DEFAULT_HTTP_OPTIONS);
	options->setWantHeaders(true);

	LLCoreHttpUtil::HttpCoroutineAdapter adapter("genericGetCoro", policy_id);
	LLSD result = adapter.getAndSuspend(url, options);

	LLCore::HttpStatus status =
		LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(result);
	if (status)
	{
		if (success)
		{
			success(result);
		}
	}
	else if (failure)
	{
		const LLSD& http_results =
			result[LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS];
		failure(http_results);
	}
}

//static
void HttpCoroutineAdapter::callbackHttpPost(const std::string& url,
											LLCore::HttpRequest::policy_t policy_id,
											const LLSD& postdata,
											completionCallback_t success,
											completionCallback_t failure)
{
	LLCoros::instance().launch("HttpCoroutineAdapter::genericPostCoro",
				  boost::bind(&HttpCoroutineAdapter::trivialPostCoro, url,
							  policy_id, postdata, success, failure));
}

//static
void HttpCoroutineAdapter::messageHttpPost(const std::string& url,
										   const LLSD& postdata,
										   const std::string& success,
										   const std::string& failure)
{
	completionCallback_t cb_succ = NULL;
	if (!success.empty())
	{
		cb_succ = (completionCallback_t)boost::bind(&logMessageSuccess,
													"HttpCoroutineAdapter",
													url, success);
	}

	completionCallback_t cb_fail = NULL;
	if (!failure.empty())
	{
		cb_fail = (completionCallback_t)boost::bind(&logMessageFail,
													"HttpCoroutineAdapter",
													url, failure);
	}

	callbackHttpPost(url, postdata, cb_succ, cb_fail);
}

//static
void HttpCoroutineAdapter::trivialPostCoro(std::string url,
										   LLCore::HttpRequest::policy_t policy_id,
										   LLSD postdata,
										   completionCallback_t success,
										   completionCallback_t failure)
{
	LL_DEBUGS("CoreHttp") << "Generic POST for: " << url << LL_ENDL;

	LLCore::HttpOptions::ptr_t options(DEFAULT_HTTP_OPTIONS);
	options->setWantHeaders(true);

	LLCoreHttpUtil::HttpCoroutineAdapter adapter("genericPostCoro", policy_id);
	LLSD result = adapter.postAndSuspend(url, postdata, options);

	LLCore::HttpStatus status =
		LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(result);
	if (status)
	{
		// If a success routine is provided call it.
		if (success)
		{
			success(result);
		}
	}
	else if (failure)
	{
		// If a failure routine is provided call it.
		const LLSD& http_results =
			result[LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS];
		failure(http_results);
	}
}

} // End namespace LLCoreHttpUtil