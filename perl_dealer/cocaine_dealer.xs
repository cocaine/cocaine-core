/** @file cocaine_dealer.xs */
// last updated: Apr.15.2012

#ifdef __cplusplus
extern "C" {
#endif
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"
#include "ppport.h"
#ifdef __cplusplus
}
#endif

#undef do_open
#undef do_close

#include <boost/lexical_cast.hpp>
#include <boost/shared_ptr.hpp>

#include <exception>
#include <string>
#include <vector>
#include <iostream>

#include "../include/cocaine/dealer/dealer.hpp"

using namespace cocaine::dealer;
using namespace std;

typedef message_path MessagePath;
typedef message_policy MessagePolicy;
typedef client Client;
typedef boost::shared_ptr<response> Response;
typedef data_container DataContainer;

// --------------------------- MESSAGE PATH --------------------------

MODULE = cocaine_dealer    PACKAGE = cocaine_dealer::message_path

PROTOTYPES: ENABLE

# ------------- MessagePath object lifetime -----------

MessagePath *
new(CLASS, service_name, handle_name)
    char* CLASS
    char* service_name
    char* handle_name
CODE:
    RETVAL = new MessagePath(service_name, handle_name);
    if (RETVAL == NULL) {
        warn("unable to malloc MessagePath");
        XSRETURN_UNDEF;
    }
OUTPUT:
    RETVAL

void
DESTROY(self)
    MessagePath* self
CODE:
    delete self;

# ------------- service_name -----------

const char *
service_name(self)
    MessagePath* self
CODE:
    RETVAL = self->service_name.c_str();
OUTPUT:
    RETVAL

void
set_service_name(self, value)
    MessagePath* self
    char* value
CODE:
    self->service_name = value;

# ------------- handle_name -----------

const char *
handle_name(self)
    MessagePath* self
CODE:
    RETVAL = self->handle_name.c_str();
OUTPUT:
    RETVAL

void
set_handle_name(self, value)
    MessagePath* self
    char* value
CODE:
    self->handle_name = value;

# --------------------------- MESSAGE POLICY --------------------------

MODULE = cocaine_dealer    PACKAGE = cocaine_dealer::message_policy

PROTOTYPES: ENABLE

# ------------- MessagePolicy object lifetime -----------

MessagePolicy *
new(CLASS, urgent, timeout, deadline, max_timeout_retries)
    char* CLASS
    bool urgent
    float timeout
    float deadline
    int max_timeout_retries
CODE:
    RETVAL = new MessagePolicy(false, urgent, false, timeout, deadline, max_timeout_retries);
    if (RETVAL == NULL) {
        warn("unable to malloc MessagePolicy");
        XSRETURN_UNDEF;
    }
OUTPUT:
    RETVAL

void
DESTROY(self)
    MessagePolicy* self
CODE:
    delete self;

# ------------- send_to_all_hosts -----------

int
send_to_all_hosts(self)
    MessagePolicy* self
CODE:
    RETVAL = self->send_to_all_hosts;
OUTPUT:
    RETVAL

void
set_send_to_all_hosts(self, value)
    MessagePolicy* self
    bool value
CODE:
    self->send_to_all_hosts = value;

# ------------- urgent -----------

int
urgent(self)
    MessagePolicy* self
CODE:
    RETVAL = self->urgent;
OUTPUT:
    RETVAL

void
set_urgent(self, value)
    MessagePolicy* self
    bool value
CODE:
    self->urgent = value;

# ------------- mailboxed -----------

int
mailboxed(self)
    MessagePolicy* self
CODE:
    RETVAL = self->mailboxed;
OUTPUT:
    RETVAL

void
set_mailboxed(self, value)
    MessagePolicy* self
    bool value
CODE:
    self->mailboxed = value;

# ------------- timeout -----------

float
timeout(self)
    MessagePolicy* self
CODE:
    RETVAL = static_cast<float>(self->timeout);
OUTPUT:
    RETVAL

void
set_timeout(self, value)
    MessagePolicy* self
    float value
CODE:
    self->timeout = static_cast<double>(value);

# ------------- deadline -----------

float
deadline(self)
    MessagePolicy* self
CODE:
    RETVAL = static_cast<float>(self->deadline);
OUTPUT:
    RETVAL

void
set_deadline(self, value)
    MessagePolicy* self
    float value
CODE:
    self->deadline = static_cast<double>(value);

# ------------- max_timeout_retries -----------

int
max_timeout_retries(self)
    MessagePolicy* self
CODE:
    RETVAL = self->max_timeout_retries;
OUTPUT:
    RETVAL

void
set_max_timeout_retries(self, value)
    MessagePolicy* self
    int value
CODE:
    self->max_timeout_retries = value;

# --------------------------- DATA CONTAINER --------------------------

MODULE = cocaine_dealer    PACKAGE = cocaine_dealer::data_container

PROTOTYPES: ENABLE

# ------------- DataContainer object lifetime -----------

DataContainer *
new(CLASS)
    char* CLASS
CODE:
    RETVAL = new DataContainer;
    if (RETVAL == NULL) {
        warn("unable to malloc DataContainer");
        XSRETURN_UNDEF;
    }
OUTPUT:
    RETVAL

void
DESTROY(self)
    DataContainer* self
CODE:
    delete self;

# ------------- data -----------

char*
data(self)
    DataContainer* self
CODE:
    RETVAL = reinterpret_cast<char*>(self->data());
OUTPUT:
    RETVAL

# ------------- size -----------

int
size(self)
    DataContainer* self
CODE:
    RETVAL = self->size();
OUTPUT:
    RETVAL


# --------------------------- RESPONSE --------------------------

MODULE = cocaine_dealer    PACKAGE = cocaine_dealer::response

PROTOTYPES: ENABLE

# ------------- Response object lifetime -----------

Response *
new(CLASS)
    char* CLASS
CODE:
    RETVAL = new Response;
    if (RETVAL == NULL) {
        warn("unable to malloc Response");
        XSRETURN_UNDEF;
    }
OUTPUT:
    RETVAL

void
DESTROY(self)
    Response* self
CODE:
    delete self;

# ------------- get -----------

int
get(self, data_container)
    Response* self
    DataContainer* data_container
CODE:
    boost::shared_ptr<response> resp = *self;
    int result = 0;

    if (resp->get(data_container)) {
        result = 1;
    }

    RETVAL = result;
OUTPUT:
    RETVAL

# --------------------------- CLIENT --------------------------

MODULE = cocaine_dealer    PACKAGE = cocaine_dealer::client

PROTOTYPES: ENABLE

# ------------- Client object lifetime -----------

Client *
new(CLASS, config_file_path)
    char* CLASS
    char* config_file_path
CODE:
    RETVAL = new Client(config_file_path);
    if (RETVAL == NULL) {
        warn("unable to malloc Client");
        XSRETURN_UNDEF;
    }
OUTPUT:
    RETVAL

void
DESTROY(self)
    Client* self
CODE:
    delete self;

# ------------- send_message -----------

int
send_message(self, data, size, path, policy, response)
    Client* self
    char* data
    int size
    MessagePath* path
    MessagePolicy* policy
    Response* response;
CODE:
    *response = self->send_message(reinterpret_cast<void*>(data), size, *path, *policy);
    RETVAL = 0;;
OUTPUT:
    RETVAL

