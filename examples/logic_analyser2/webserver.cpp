//
// webserver.cpp
//
// Circle - A C++ bare metal environment for Raspberry Pi
// Copyright (C) 2015-2026  R. Stange <rsta2@gmx.net>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
#include "webserver.h"
#include <circle/logger.h>
#include <circle/util.h>
#include <assert.h>

#define MAX_CONTENT_SIZE	(MAX_MEMORY_DEPTH / 2)		// TODO: for 4 channels only
#define MAX_FORMDATA_SIZE	2000

#define SOCKET_TIMEOUT_SECS	10

// our content
static const char s_Index[] =
#include "index.h"
;

static const u8 s_Style[] =
#include "style.h"
;

static const u8 s_Favicon[] =
{
#include "favicon.h"
};

LOGMODULE ("webserver");

CWebServer::CWebServer (CNetSubSystem *pNetSubSystem, CRecorder *pRecorder, CSocket *pSocket)
:	CHTTPDaemon (pNetSubSystem, pSocket, MAX_CONTENT_SIZE, HTTP_PORT,
		     MAX_FORMDATA_SIZE, SOCKET_TIMEOUT_SECS),
	m_pRecorder (pRecorder)
{
}

CWebServer::~CWebServer (void)
{
}

CHTTPDaemon *CWebServer::CreateWorker (CNetSubSystem *pNetSubSystem, CSocket *pSocket)
{
	return new CWebServer (pNetSubSystem, m_pRecorder, pSocket);
}

THTTPStatus CWebServer::GetContent (const char  *pPath,
				    const char  *pParams,
				    const char  *pFormData,
				    u8	        *pBuffer,
				    unsigned    *pLength,
				    const char **ppContentType)
{
	assert (pPath != 0);
	assert (pFormData != 0);
	assert (ppContentType != 0);
	assert (m_pRecorder != 0);

	const u8 *pContent = 0;
	unsigned nLength = 0;

	if (   strcmp (pPath, "/") == 0
	    || strcmp (pPath, "/index.html") == 0)
	{
		pContent = (const u8 *) s_Index;
		nLength = sizeof s_Index;
		*ppContentType = "text/html; charset=iso-8859-1";
	}
	else if (strcmp (pPath, "/logic_analyser") == 0)
	{
		char FormData[strlen (pFormData)+1];
		strcpy (FormData, pFormData);

		unsigned nParam;
		if ((nParam = GetParam ("depth", 64, MAX_MEMORY_DEPTH, FormData)) == InvalidParam)
		{
			return HTTPBadRequest;
		}
		m_pRecorder->SetMemoryDepth (nParam);

		if ((nParam = GetParam ("rate", MIN_SAMPLE_RATE, MAX_SAMPLE_RATE)) == InvalidParam)
		{
			return HTTPBadRequest;
		}
		m_pRecorder->SetClockDivider ((float) MAX_SAMPLE_RATE / nParam);

		if ((nParam = GetParam ("trigger_channel", 0, 4)) == InvalidParam)
		{
			return HTTPBadRequest;
		}
		unsigned nTriggerChannel = nParam;

		if ((nParam = GetParam ("trigger_level", 0, 1)) == InvalidParam)
		{
			return HTTPBadRequest;
		}
		m_pRecorder->SetTrigger (!!nTriggerChannel, nTriggerChannel, !!nParam);

		if (!m_pRecorder->Run ())
		{
			return HTTPRequestTimeout;
		}

		pContent = (const u8 *) m_pRecorder->GetSampleBuffer ();
		nLength = m_pRecorder->GetSampleBufferSize ();
		*ppContentType = "application/octet-stream";
	}
	else if (strcmp (pPath, "/style.css") == 0)
	{
		pContent = s_Style;
		nLength = sizeof s_Style-1;
		*ppContentType = "text/css";
	}
	else if (strcmp (pPath, "/favicon.ico") == 0)
	{
		pContent = s_Favicon;
		nLength = sizeof s_Favicon;
		*ppContentType = "image/x-icon";
	}
	else
	{
		return HTTPNotFound;
	}

	assert (pLength != 0);
	if (*pLength < nLength)
	{
		LOGERR ("Increase MAX_CONTENT_SIZE to at least %u", nLength);

		return HTTPInternalServerError;
	}

	assert (pBuffer != 0);
	assert (pContent != 0);
	assert (nLength > 0);
	memcpy (pBuffer, pContent, nLength);

	*pLength = nLength;

	return HTTPOK;
}

unsigned CWebServer::GetParam (const char *pName, unsigned nMin, unsigned nMax, char *pFormData)
{
	assert (pName);
	assert (nMin <= nMax);
	assert (nMax < InvalidParam);

	const char *pParam = strtok_r (pFormData, "=", &m_pSavePtr);
	if (!pParam)
	{
		return InvalidParam;
	}

	if (strcmp (pParam, pName) != 0)
	{
		return InvalidParam;
	}

	pParam = strtok_r (nullptr, "&", &m_pSavePtr);
	if (!pParam)
	{
		return InvalidParam;
	}

	char *pEnd = 0;
	unsigned long ulParam = strtoul (pParam, &pEnd, 0);
	if (pEnd && *pEnd)
	{
		return InvalidParam;
	}

	if (!(nMin <= ulParam && ulParam <= nMax))
	{
		return InvalidParam;
	}

	// LOGDBG ("%s=%lu", pName, ulParam);

	return ulParam;
}
