/*
 * Author: Jacky
*/

#include <dlfcn.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/poll.h>
#include <fnmatch.h>
#include <stdint.h>

#include "logging.h"
#include "transport.h"
#include "list.h"
#include "socket.h"

transport_t *
transport_load (dict_t *options)
{
	struct transport *trans = NULL, *return_trans = NULL;
	char *name = NULL;
	void *handle = NULL;
	char *type = NULL;
	char str[] = "ERROR";
	int32_t ret = -1;
	int8_t is_tcp = 0, is_unix = 0, is_ibsdp = 0, is_msocket = 0;
	volume_opt_list_t *vol_opt = NULL;

	LZ_VALIDATE_OR_GOTO("transport", options, fail);
  
	trans = CALLOC (1, sizeof (struct transport));
	LZ_VALIDATE_OR_GOTO("transport", trans, fail);

	type = str;

	/* Backward compatibility */
	ret = dict_get_str (options, "transport-type", &type);
	if (ret < 0) {
		ret = dict_set_str (options, "transport-type", "socket");
		if (ret < 0)
			lz_log ("dict", LZ_LOG_DEBUG,
				"setting transport-type failed");
		lz_log ("transport", LZ_LOG_WARNING,
			"missing 'option transport-type'. defaulting to "
			"\"socket\"");
	} else {
		{
			/* Backword compatibility to handle * /client,
			 * * /server. 
			 */
			char *tmp = strchr (type, '/');
			if (tmp)
				*tmp = '\0';
		}
		
		is_tcp = strcmp (type, "tcp");
        if(is_tcp == 0)
            ret = dict_set_str (options, 
                    "transport.address-family",
                    "inet");
        if (ret < 0)
            lz_log ("dict", LZ_LOG_DEBUG,
                    "setting transport-type failed");
	}

    trans->ops = epoll_ops;

    trans->init = socket_init;

    trans->fini = socket_fini;
	
	ret = trans->init (trans, options);
	if (ret != 0) {
		lz_log ("transport", LZ_LOG_ERROR,
			"'%s' initialization failed", type);
		FREE (trans);
		goto fail;
	}

	pthread_mutex_init (&trans->lock, NULL);
	return_trans = trans;
fail:
	return return_trans;
}


int32_t 
transport_submit (transport_t *this, char *buf, int32_t len,
		  struct iovec *vector, int count,
                  struct iobref *iobref)
{
    int32_t               ret = -1;
/*
    transport_t          *peer_trans = NULL;
    struct iobuf         *iobuf = NULL;
    struct transport_msg *msg = NULL;
    if (this->peer_trans) {
        peer_trans = this->peer_trans;

        msg = CALLOC (1, sizeof (*msg));
        if (!msg) {
            return -ENOMEM;
        }

        msg->hdr = buf;
        msg->hdrlen = len;

        if (vector) {
            //todo modify get iobuf_pool
            iobuf = iobuf_get (iobuf_pool);
            if (!iobuf) {
                FREE (msg->hdr);
                FREE (msg);
                return -ENOMEM;
            }

            iov_unload (iobuf->ptr, vector, count);
            msg->iobuf = iobuf;
        }

        pthread_mutex_lock (&peer_trans->handover.mutex);
        {
            list_add_tail (&msg->list, &peer_trans->handover.msgs);
            pthread_cond_broadcast (&peer_trans->handover.cond);
        }
        pthread_mutex_unlock (&peer_trans->handover.mutex);

        return 0;
    }
*/
    LZ_VALIDATE_OR_GOTO("transport", this, fail);
    LZ_VALIDATE_OR_GOTO("transport", this->ops, fail);

    ret = this->ops->submit (this, buf, len, vector, count, iobref);
fail:
    return ret;
}


int32_t 
transport_connect (transport_t *this)
{
	int ret = -1;
	
	LZ_VALIDATE_OR_GOTO("transport", this, fail);
  
	ret = this->ops->connect (this);
fail:
	return ret;
}


int32_t
transport_listen (transport_t *this)
{
	int ret = -1;
	
	LZ_VALIDATE_OR_GOTO("transport", this, fail);
  
	ret = this->ops->listen (this);
fail:
	return ret;
}


int32_t 
transport_disconnect (transport_t *this)
{
	int32_t ret = -1;
	
	LZ_VALIDATE_OR_GOTO("transport", this, fail);
  
	ret = this->ops->disconnect (this);
fail:
	return ret;
}


int32_t 
transport_destroy (transport_t *this)
{
	int32_t ret = -1;

	LZ_VALIDATE_OR_GOTO("transport", this, fail);
  
	if (this->fini)
		this->fini (this);

	pthread_mutex_destroy (&this->lock);
	FREE (this);
fail:
	return ret;
}


transport_t *
transport_ref (transport_t *this)
{
	transport_t *return_this = NULL;

	LZ_VALIDATE_OR_GOTO("transport", this, fail);
	
	pthread_mutex_lock (&this->lock);
	{
		this->refcount ++;
	}
	pthread_mutex_unlock (&this->lock);
	
	return_this = this;
fail:
	return return_this;
}


int32_t
transport_receive (transport_t *this, char **hdr_p, size_t *hdrlen_p,
		   struct iobuf **iobuf_p)
{
	int32_t ret = -1;

	LZ_VALIDATE_OR_GOTO("transport", this, fail);

        if (this->peer_trans) {
                *hdr_p = this->handover.msg->hdr;
                *hdrlen_p = this->handover.msg->hdrlen;
                *iobuf_p = this->handover.msg->iobuf;

                return 0;
        }

	ret = this->ops->receive (this, hdr_p, hdrlen_p, iobuf_p);
fail:
	return ret;
}


int32_t
transport_unref (transport_t *this)
{
	int32_t refcount = 0;
	int32_t ret = -1;

	LZ_VALIDATE_OR_GOTO("transport", this, fail);
  
	pthread_mutex_lock (&this->lock);
	{
		refcount = --this->refcount;
	}
	pthread_mutex_unlock (&this->lock);

	if (refcount == 0) {
		xlator_notify (this->xl, LZ_EVENT_TRANSPORT_CLEANUP, this);
		transport_destroy (this);
	}
	
	ret = 0;
fail:
	return ret;
}

/*
void *
transport_peerproc (void *trans_data)
{
    transport_t          *trans = NULL;
    struct transport_msg *msg = NULL;

    trans = trans_data;

    while (1) {
        pthread_mutex_lock (&trans->handover.mutex);
        {
            while (list_empty (&trans->handover.msgs))
                pthread_cond_wait (&trans->handover.cond,
                        &trans->handover.mutex);

            msg = list_entry (trans->handover.msgs.next,
                    struct transport_msg, list);

            list_del_init (&msg->list);
        }
        pthread_mutex_unlock (&trans->handover.mutex);

        trans->handover.msg = msg;

        xlator_notify (trans->xl, LZ_EVENT_POLLIN, trans);

        FREE (msg);
    }
}


int
transport_setpeer (transport_t *trans, transport_t *peer_trans)
{
        trans->peer_trans = transport_ref (peer_trans);

        INIT_LIST_HEAD (&trans->handover.msgs);
        pthread_cond_init (&trans->handover.cond, NULL);
        pthread_mutex_init (&trans->handover.mutex, NULL);
        pthread_create (&trans->handover.thread, NULL,
                        transport_peerproc, trans);

        peer_trans->peer_trans = transport_ref (trans);

        INIT_LIST_HEAD (&peer_trans->handover.msgs);
        pthread_cond_init (&peer_trans->handover.cond, NULL);
        pthread_mutex_init (&peer_trans->handover.mutex, NULL);
        pthread_create (&peer_trans->handover.thread, NULL,
                        transport_peerproc, peer_trans);

        return 0;
}
*/
