//------------------------------------------------------------------------------
// Copyright (C) 2011, Robert Johansson, Raditex AB
// All rights reserved.
//
// rSCADA
// http://www.rSCADA.se
// info@rscada.se
//
//------------------------------------------------------------------------------

#include <string.h>

#include <stdio.h>
#include <mbus/mbus.h>

static int debug = 0;

//
// init slave to get really the beginning of the records
//
static int
init_slaves(mbus_handle *handle)
{
    if (debug)
        printf("%s: debug: sending init frame #1\n", __PRETTY_FUNCTION__);

    if (mbus_send_ping_frame(handle, MBUS_ADDRESS_NETWORK_LAYER, 1) == -1)
    {
        return 0;
    }

    //
    // resend SND_NKE, maybe the first get lost
    //

    if (debug)
        printf("%s: debug: sending init frame #2\n", __PRETTY_FUNCTION__);

    if (mbus_send_ping_frame(handle, MBUS_ADDRESS_NETWORK_LAYER, 1) == -1)
    {
        return 0;
    }

    return 1;
}

//------------------------------------------------------------------------------
// Scan for devices using secondary addressing.
//------------------------------------------------------------------------------
int
main(int argc, char **argv)
{
    int result = 1;

    mbus_frame reply;
    mbus_frame_data reply_data;
    mbus_handle *handle = NULL;

    char *device, *addr_str, *xml_result;
    int address;
    long baudrate = 2400;
    int i;

    memset((void *)&reply, 0, sizeof(mbus_frame));
    memset((void *)&reply_data, 0, sizeof(mbus_frame_data));

    if (argc == 3)
    {
        device   = argv[1];
        addr_str = argv[2];
    }
    else if (argc == 4 && strcmp(argv[1], "-d") == 0)
    {
        device   = argv[2];
        addr_str = argv[3];
        debug = 1;
    }
    else if (argc == 5 && strcmp(argv[1], "-b") == 0)
    {
        baudrate = atol(argv[2]);
        device   = argv[3];
        addr_str = argv[4];
    }
    else if (argc == 6 && strcmp(argv[1], "-d") == 0 && strcmp(argv[2], "-b") == 0)
    {
        baudrate = atol(argv[3]);
        device   = argv[4];
        addr_str = argv[5];
        debug = 1;
    }
    else
    {
        fprintf(stderr, "usage: %s [-d] [-b BAUDRATE] device mbus-address\n", argv[0]);
        fprintf(stderr, "    optional flag -d for debug printout\n");
        fprintf(stderr, "    optional flag -b for selecting baudrate\n");
        return 0;
    }

    if ((handle = mbus_context_serial(device)) == NULL)
    {
        fprintf(stderr, "Could not initialize M-Bus context: %s\n",  mbus_error_str());
        return 1;
    }
    
    if (debug)
    {
        mbus_register_send_event(handle, &mbus_dump_send_event);
        mbus_register_recv_event(handle, &mbus_dump_recv_event);
    }

    if (mbus_connect(handle) == -1)
    {
        fprintf(stderr,"Failed to setup connection to M-bus gateway\n");
        goto FAILC;
    }

    if (mbus_serial_set_baudrate(handle, baudrate) == -1)
    {
        fprintf(stderr,"Failed to set baud rate to %ld.\n", baudrate);
        goto FAIL;
    }

#if 0
    if (init_slaves(handle) == 0)
    {
        goto FAIL;
    }
#endif

    // primary addressing
    address = atoi(addr_str);

#if 0
    // Application reset without subcode
    if (mbus_send_application_reset_frame(handle, address, -1) == -1)
    {
        fprintf(stderr, "Failed to send M-Bus app reset frame.\n");
        goto FAIL;
    }

    // Get answer to app reset
    if (mbus_purge_frames(handle) != 1)
    {
        fprintf(stderr, "Failed to receive M-Bus response to app reset.\n");
    }
#endif

    // Send Landis+Gyr REQ_UD2 special variant to get answer with 9600 baud 
    if (mbus_send_request_frame_9600(handle, address) == -1)
    {
        fprintf(stderr, "Failed to send M-Bus request frame.\n");
        goto FAIL;
    }

    // Switch to 9600 baud
    mbus_disconnect(handle);
    mbus_connect(handle);
    baudrate = 9600;
    if (mbus_serial_set_baudrate(handle, baudrate) == -1)
    {
        fprintf(stderr,"Failed to set baud rate to %ld.\n", baudrate);
        goto FAIL;
    }

    // Wait long for answer by retrying 10 times, answer is delayed by ~600ms
    for (i=0; i<10; i++)
    {
        if (mbus_recv_frame(handle, &reply) == MBUS_RECV_RESULT_OK)
        {
            break;
        }
    }
    if (i>=10)
    {
        fprintf(stderr, "Failed to receive M-Bus response frame.\n");
        goto FAIL;
    }

    //
    // dump hex data if debug is true
    //
    if (debug)
    {
        mbus_frame_print(&reply);
    }

    //
    // parse data
    //
    if (mbus_frame_data_parse(&reply, &reply_data) == -1)
    {
        fprintf(stderr, "M-bus data parse error: %s\n", mbus_error_str());
        goto FAIL;
    }

    //
    // generate XML and print to standard output
    //
    if ((xml_result = mbus_frame_data_xml(&reply_data)) == NULL)
    {
        fprintf(stderr, "Failed to generate XML representation of MBUS frame: %s\n", mbus_error_str());
        goto FAIL;
    }

    printf("%s", xml_result);
    free(xml_result);

    // manual free
    if (reply_data.data_var.record)
    {
        mbus_data_record_free(reply_data.data_var.record); // free's up the whole list
    }

    result = 0;
FAIL:
    mbus_disconnect(handle);
FAILC:
    mbus_context_free(handle);
    return result;
}

