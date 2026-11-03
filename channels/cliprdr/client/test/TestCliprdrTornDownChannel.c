/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Regression test: cliprdr_packet_send must not write through a torn-down
 * channel handle.
 *
 * CHANNEL_EVENT_DISCONNECTED (cliprdr_virtual_channel_event_disconnected)
 * zeroes cliprdrPlugin::OpenHandle after closing the channel, but another
 * thread (e.g. the X11 clipboard-owner poll during auto-reconnect) can still
 * have a send in flight. Before the fix, cliprdr_packet_send() dereferenced
 * channelEntryPoints.pVirtualChannelWriteEx unconditionally once OpenHandle
 * reached that state, handing the channel manager a stale handle (observed
 * in the field as CHANNEL_RC_BAD_CHANNEL_HANDLE from VirtualChannelWrite).
 */

#include <winpr/crt.h>
#include <winpr/stream.h>
#include <winpr/wlog.h>

#include <freerdp/channels/cliprdr.h>

#include "../cliprdr_main.h"

static UINT VCAPITYPE stub_virtual_channel_write_ex(LPVOID pInitHandle, DWORD openHandle,
                                                    LPVOID pData, ULONG dataLength,
                                                    LPVOID pUserData)
{
	WINPR_UNUSED(pInitHandle);
	WINPR_UNUSED(openHandle);
	WINPR_UNUSED(pData);
	WINPR_UNUSED(dataLength);

	/* Mimic the real channel manager taking ownership of the stream. */
	if (pUserData)
		Stream_Free((wStream*)pUserData, TRUE);
	return CHANNEL_RC_OK;
}

static BOOL test_send_on_torn_down_channel(void)
{
	cliprdrPlugin cliprdr = { 0 };
	cliprdr.log = WLog_Get("test.cliprdr.torndown");
	cliprdr.channelEntryPoints.pVirtualChannelWriteEx = stub_virtual_channel_write_ex;

	/* Simulate the state right after CHANNEL_EVENT_DISCONNECTED. */
	cliprdr.OpenHandle = 0;

	const UINT status = cliprdr_send_error_response(&cliprdr, CB_FORMAT_LIST);
	if (status != CHANNEL_RC_BAD_CHANNEL_HANDLE)
	{
		(void)fprintf(stderr,
		              "test_send_on_torn_down_channel: expected CHANNEL_RC_BAD_CHANNEL_HANDLE "
		              "(0x%08X), got 0x%08X\n",
		              CHANNEL_RC_BAD_CHANNEL_HANDLE, status);
		return FALSE;
	}

	return TRUE;
}

static BOOL test_send_on_open_channel(void)
{
	cliprdrPlugin cliprdr = { 0 };
	cliprdr.log = WLog_Get("test.cliprdr.open");
	cliprdr.channelEntryPoints.pVirtualChannelWriteEx = stub_virtual_channel_write_ex;

	/* A normal, still-open channel must keep sending as before. */
	cliprdr.OpenHandle = 1;

	const UINT status = cliprdr_send_error_response(&cliprdr, CB_FORMAT_LIST);
	if (status != CHANNEL_RC_OK)
	{
		(void)fprintf(stderr, "test_send_on_open_channel: expected CHANNEL_RC_OK, got 0x%08X\n",
		              status);
		return FALSE;
	}

	return TRUE;
}

int TestCliprdrTornDownChannel(int argc, char* argv[])
{
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	if (!test_send_on_torn_down_channel())
		return -1;

	if (!test_send_on_open_channel())
		return -1;

	return 0;
}
