/*
 * HTTP.cpp
 *
 *  Created on: 23/lug/2013
 *      Author: stefano
 */

#include "HTTP.h"
#include "HTTPDefines.h"
#include "HTTPRequestHeader.h"
#include "HTTPResponseHeader.h"

#include <sys/socket.h>

#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <iostream>
#include <sstream>

HTTP::HTTP()
	:
	fPort(-1),
	fFD(-1),
	fLastError(0)
{
}


HTTP::HTTP(const std::string string, int port)
	:
	fPort(-1),
	fFD(-1),
	fLastError(0)
{
	std::string hostName = _HostFromConnectionString(string);
	fHost = hostName;
	fPort = port;
}


HTTP::~HTTP()
{
	Close();
}


void
HTTP::Close()
{
	if (fFD > 0) {
		::close(fFD);
		fFD = -1;
	}
}


void
HTTP::ClearPendingRequests()
{
}


HTTPRequestHeader
HTTP::CurrentRequest() const
{
	return HTTPRequestHeader();
}


int
HTTP::Error() const
{
	return fLastError;
}


std::string
HTTP::ErrorString() const
{
	return strerror(fLastError);
}


int
HTTP::Get(const std::string path)
{
	if (!_HandleConnectionIfNeeded(path))
		return -1;

	std::string fullPath = fHost + path;
	HTTPRequestHeader requestHeader("GET", fullPath);
	fCurrentRequest = requestHeader;
	std::string string = requestHeader.ToString();

#if 1
	std::cout << "HTTP::GET: header:" << std::endl << string << std::endl;
#endif
	if (::write(fFD, string.c_str(), string.length())
			!= (int)string.length()) {
		fLastError = errno;
		return errno;
	}

	std::ostringstream reply;
	char buffer[256];
	size_t sizeRead = 0;
	while ((sizeRead = ::read(fFD, buffer, sizeof(buffer))) > 0) {
		reply.write(buffer, sizeRead);
	}

	std::string replyString = reply.str();
	// Read back status
	size_t pos = replyString.find('\012');
	if (pos == std::string::npos) {
		fLastError = -1;
		return -1;
	}


	std::string statusLine = replyString.substr(0, pos);
	int code;
	::sscanf(statusLine.c_str(), "HTTP/1.%*d %03d", (int*)&code);

	pos = replyString.find(HTTPContentLength);
	if (pos != std::string::npos) {
		size_t endPos = replyString.find('\012', pos);
		std::string contentLengthString = replyString.substr(pos, endPos);
		std::cout << "Content length: " << contentLengthString << std::endl;
	}


	fLastResponse.SetStatusLine(code, statusLine.c_str());

	// TODO: Read actual data from the ostringstream object

	return 0;
}


HTTPResponseHeader
HTTP::LastResponse() const
{
	return fLastResponse;
}



int
HTTP::SetHost(const std::string hostName, int port)
{
	fHost = _HostFromConnectionString(hostName);
	fPort = port;
	fLastError = 0;

	std::cout << "Host set to " << fHost << std::endl;
	return 0;
}


int
HTTP::Post(const std::string path, char* data)
{
	if (fFD < 0)
		return -1;

	std::string fullPath = fHost + path;
	HTTPRequestHeader requestHeader("POST", fullPath);
	fCurrentRequest = requestHeader;
	std::string string = requestHeader.ToString();

#if 1
	std::cout << "HTTP::POST: header:" << std::endl << string << std::endl;
#endif
	if (::write(fFD, string.c_str(), string.length())
			!= (int)string.length()) {
		fLastError = -1;
		return -1;
	}

	std::ostringstream reply;
	char buffer[256];
	size_t sizeRead = 0;
	while ((sizeRead = ::read(fFD, buffer, sizeof(buffer))) > 0) {
		reply.write(buffer, sizeRead);
	}

	std::string replyString = reply.str();
	// Read back status
	size_t pos = replyString.find('\012');
	if (pos == std::string::npos) {
		fLastError = -1;
		return -1;
	}


	std::string statusLine = replyString.substr(0, pos);
	int code;
	::sscanf(statusLine.c_str(), "HTTP/1.%*d %03d", (int*)&code);

	pos = replyString.find(HTTPContentLength);
	if (pos != std::string::npos) {
		size_t endPos = replyString.find('\012', pos);
		std::string contentLengthString = replyString.substr(pos, endPos);
		std::cout << "Content length: " << contentLengthString << std::endl;
	}


	fLastResponse.SetStatusLine(code, statusLine.c_str());

	// TODO: Read actual data from the ostringstream object
	return -1;
}


int
HTTP::Request(HTTPRequestHeader& header, const void* data)
{
	if (!_HandleConnectionIfNeeded(header.Value(HTTPHost)))
		return -1;

	fCurrentRequest = header;
	std::string string = header.ToString();

#if 1
	std::cout << "HTTP::POST: header:" << std::endl << string << std::endl;
#endif
	if (::write(fFD, string.c_str(), string.length())
			!= (int)string.length()) {
		fLastError = -1;
		return -1;
	}

	return -1;
}


std::string
HTTP::_HostFromConnectionString(std::string string) const
{
	// TODO: Remove port if specified
	size_t prefixPos = string.find(HTTPProtocolPrefix);
	if (prefixPos == std::string::npos)
		return string;

	return string.substr(HTTPProtocolPrefix.length(), std::string::npos);
}


bool
HTTP::_HandleConnectionIfNeeded(const std::string string, const int port)
{
	std::string hostName = _HostFromConnectionString(string);

	if (fFD >= 0) {
		if (hostName == "" || (hostName == fHost && port == fPort)) {
			// TODO: Return an "already connected" error, or close and open a new
			// connection
			std::cerr << "HTTP::Connect: already connected" << std::endl;
			return true;
		}
		close(fFD);
		fFD = -1;
	}

	if (hostName != "") {
		fHost = hostName;
		fPort = port;
	}
	std::cout << "_HandleConnectionIfNeeded: connect to ";
	std::cout << fHost << std::endl;

	struct hostent* hostEnt = ::gethostbyname(fHost.c_str());
	if (hostEnt == NULL) {
		fLastError = h_errno;
		return false;
	}

	if ((fFD = ::socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		fLastError = errno;
		return false;
	}

	::setsockopt(fFD, SOL_SOCKET, SO_KEEPALIVE, 0, 0);

	struct sockaddr_in serverAddr;
	::memset((char*)&serverAddr,0, sizeof(serverAddr));
	::memcpy((char*)&serverAddr.sin_addr, hostEnt->h_addr, hostEnt->h_length);
	serverAddr.sin_family = hostEnt->h_addrtype;
	serverAddr.sin_port = (unsigned short)htons(fPort);

	if (::connect(fFD, (const struct sockaddr*)&serverAddr,
			sizeof(serverAddr)) < 0) {
		fLastError = errno;
		return false;
	}

	fLastError = 0;

	return true;
}