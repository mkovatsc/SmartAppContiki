/* request.c */
#include <sml/sml_transport.h>
#include <sml/sml_file.h>
#include <sml/sml_open_request.h>
#include <sml/sml_close_request.h>
#include <sml/sml_tree.h>

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <termios.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>

#include <sml/sml_file.h>
#include <sml/sml_transport.h>

static int fd;

int serial_port_open(const char* device) {
    int bits;
    struct termios config;
    memset(&config, 0, sizeof(config));
    
    int fd = open(device, O_RDWR | O_NOCTTY | O_NDELAY);
    if (fd < 0) {
        printf("error: open(%s): %s\n", device, strerror(errno));
        return -1;
    }
    
    // set RTS
    ioctl(fd, TIOCMGET, &bits);
    bits |= TIOCM_RTS;
    ioctl(fd, TIOCMSET, &bits);
    
    tcgetattr( fd, &config ) ;
    
    // set 8-N-1
    config.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
    config.c_oflag &= ~OPOST;
    config.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    config.c_cflag &= ~(CSIZE | PARENB | PARODD | CSTOPB);
    config.c_cflag |= CS8;
    
    // set speed to 9600 baud
    cfsetispeed( &config, B9600);
    cfsetospeed( &config, B9600);
    
    tcsetattr(fd, TCSANOW, &config);
    return fd;
}
static int number = 0;

void transport_receiver(unsigned char *buffer, size_t buffer_len) {
    // the buffer contains the whole message, with transport escape sequences.
    // these escape sequences are stripped here.
    printf("received sml file %d...\n", ++number);
    sml_file *file = sml_file_parse(buffer + 8, buffer_len - 16);
    // the sml file is parsed now
    
    // read here some values ..
    
    // this prints some information about the file
    sml_file_print(file);
    int i,j;
    for(i=0;i<file->messages_len;i++)
    {
        if(*(file->messages[i]->message_body->tag) == SML_MESSAGE_ATTENTION_RESPONSE)
        {
            sml_attention_response *r = (sml_attention_response*) file->messages[i]->message_body->data;
            hexdump(r->attention_number->str,r->attention_number->len);
            if(r->attention_message)
            {
                printf("MESSAGE\n");
            }
            if(r->attention_details)
            {
                printf("DETAILS\n");
            }
        }
        if(*(file->messages[i]->message_body->tag) == SML_MESSAGE_GET_PROC_PARAMETER_RESPONSE)
        {
            printf("GIGITTY!\n");
        }
    }
    // free the malloc'd memory
    sml_file_free(file);
}

void *sml_listen()
{
    sml_transport_listen(fd,transport_receiver);
}


int main(int argc, char *argv[])
{
    char *device = "/dev/ttyUSB0";
    fd = serial_port_open(device);
    int bar;
    pthread_t listener_thread;
    
    if (fd > 0) 
    {
        pthread_create(&listener_thread,NULL,sml_listen,NULL);
        while(1)
        {
            sml_file *file = sml_file_init();
            sml_message *open = sml_message_init();
            open->group_id = sml_u8_init(1);
            open->abort_on_error = sml_u8_init(0);
            sml_open_request *request = sml_open_request_init();
            request->client_id   = sml_octet_string_init_from_hex("010203040506");
            request->req_file_id = sml_octet_string_init_from_hex("51");   
            request->server_id   = sml_octet_string_init_from_hex("0901HAG000003_DC");
            request->username = sml_octet_string_init_from_hex("0000");
            request->password = sml_octet_string_init_from_hex("1234");
            request->sml_version = sml_u8_init(1);
            open->message_body = sml_message_body_init(SML_MESSAGE_OPEN_REQUEST, request);
            sml_file_add_message(file, open);
            
            sml_message *proc = sml_message_init();
            proc->group_id = sml_u8_init(2);
            proc->abort_on_error = sml_u8_init(0);
            sml_get_proc_parameter_request *procrequest = sml_get_proc_parameter_request_init();
            procrequest->server_id = sml_octet_string_init_from_hex("0901HAG000003_DC");
            sml_tree_path *path = sml_tree_path_init();
            octet_string *obis = sml_octet_string_init_from_hex("8181C78C02FF");
            sml_tree_path_add_path_entry(path, obis);
            procrequest->parameter_tree_path = path;
            proc->message_body = sml_message_body_init(SML_MESSAGE_GET_PROC_PARAMETER_REQUEST, procrequest);
            sml_file_add_message(file, proc);
            
            sml_message *close = sml_message_init();
            close->group_id = sml_u8_init(3);
            close->abort_on_error = sml_u8_init(0);
            sml_close_request *crequest = sml_close_request_init();
            close->message_body = sml_message_body_init(SML_MESSAGE_CLOSE_REQUEST, crequest);
            sml_file_add_message(file, close);
            sml_transport_write(fd, file);
            sml_file_free(file);
            scanf("%d", &bar);
        }
    }
    
    return 0;
}