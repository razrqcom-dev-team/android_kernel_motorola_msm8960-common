#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/major.h>
#include <linux/tty.h>
#include <linux/tty_driver.h>
#include <linux/tty_flip.h>

#include <mach/tty_diag.h>

#define DIAG_MAJOR 185
#define DIAG_TTY_MINOR_COUNT 2

static DEFINE_MUTEX(diag_tty_lock);

static struct tty_driver *diag_tty_driver;
static struct legacy_diag_ch legacy_ch;
static struct diag_request *d_req_ptr;

struct diag_tty_data {
	struct tty_struct *tty;
	int open_count;
};

static struct diag_tty_data diag_tty[DIAG_TTY_MINOR_COUNT];

static int diag_tty_open(struct tty_struct *tty, struct file *f)
{
	int n = tty->index;
	struct diag_tty_data *tty_data;

	tty_data = diag_tty + n;

	if (n < 0 || n >= DIAG_TTY_MINOR_COUNT)
		return -ENODEV;

	/* Diag kernel driver not ready */
	if (!(legacy_ch.priv))
		return -EAGAIN;

	if (tty_data->open_count >= 1)
		return -EBUSY;

	mutex_lock(&diag_tty_lock);

	tty_data->tty = tty;
	tty->driver_data = tty_data;
	tty_data->open_count++;

	mutex_unlock(&diag_tty_lock);

	legacy_ch.notify(legacy_ch.priv, CHANNEL_DIAG_CONNECT, NULL);

	return 0;
}

static void diag_tty_close(struct tty_struct *tty, struct file *f)
{
	struct diag_tty_data *tty_data = tty->driver_data;
	int disconnect_channel = 1;
	int i;

	if (tty_data == NULL)
		return;

	mutex_lock(&diag_tty_lock);
	tty_data->open_count--;

	for (i = 0; i < DIAG_TTY_MINOR_COUNT; i++) {
		if (diag_tty[i].open_count) {
			disconnect_channel = 0;
			break;
		}
	}

	if (disconnect_channel && legacy_ch.notify && legacy_ch.priv)
		legacy_ch.priv_channel = NULL;

	if (tty_data->open_count == 0)
		tty->driver_data = NULL;

	mutex_unlock(&diag_tty_lock);
}

static int diag_tty_write(struct tty_struct *tty,
				const unsigned char *buf, int len)
{
	struct diag_tty_data *tty_data = tty->driver_data;
	struct diag_request *d_req_temp = d_req_ptr;

	mutex_lock(&diag_tty_lock);

	/* Make sure diag char driver is ready and no outstanding request */
	if ((d_req_ptr == NULL) || legacy_ch.priv_channel) {
		mutex_unlock(&diag_tty_lock);
		return -EAGAIN;
	}

	/* Diag packet must fit in buff and be written all at once */
	if (len > d_req_ptr->length) {
		mutex_unlock(&diag_tty_lock);
		return -EMSGSIZE;
	}

	d_req_ptr->actual = len;
	memcpy(d_req_ptr->buf, buf, len);

	/* Set active tty for responding */
	legacy_ch.priv_channel = tty_data;

	mutex_unlock(&diag_tty_lock);

	legacy_ch.notify(legacy_ch.priv, CHANNEL_DIAG_READ_DONE, d_req_temp);

	return len;
}

static int diag_tty_write_room(struct tty_struct *tty)
{
	if ((d_req_ptr == NULL) || legacy_ch.priv_channel)
		return 0;
	else
		return d_req_ptr->length;
}

static int diag_tty_chars_in_buffer(struct tty_struct *tty)
{
	/* Data is always written when available, this should be changed if
	   write to userspace is changed to delayed work */
	return 0;
}

static const struct tty_operations diag_tty_ops = {
	.open = diag_tty_open,
	.close = diag_tty_close,
	.write = diag_tty_write,
	.write_room = diag_tty_write_room,
	.chars_in_buffer = diag_tty_chars_in_buffer,
};

/* Diag char driver ready */
struct legacy_diag_ch *tty_diag_channel_open(const char *name, void *priv,
		void (*notify)(void *, unsigned, struct diag_request *))
{
	int i;

	if (legacy_ch.priv != NULL)
		return ERR_PTR(-EBUSY);

	mutex_lock(&diag_tty_lock);
	legacy_ch.priv = priv;
	legacy_ch.notify = notify;
	mutex_unlock(&diag_tty_lock);

	for (i = 0; i < DIAG_TTY_MINOR_COUNT; i++)
		tty_register_device(diag_tty_driver, i, NULL);

	return &legacy_ch;
}
EXPORT_SYMBOL(tty_diag_channel_open);

/* Diag char driver no longer ready */
void tty_diag_channel_close(struct legacy_diag_ch *diag_ch)
{
	struct diag_tty_data *priv_channel = diag_ch->priv_channel;
	int i;

	if (diag_ch->priv_channel) {
		tty_insert_flip_char(priv_channel->tty, 0x00, TTY_BREAK);
		tty_flip_buffer_push(priv_channel->tty);
	}
	mutex_lock(&diag_tty_lock);
	diag_ch->priv = NULL;
	diag_ch->notify = NULL;

	for (i = DIAG_TTY_MINOR_COUNT - 1; i >= 0; i--)
		tty_unregister_device(diag_tty_driver, i);

	mutex_unlock(&diag_tty_lock);
}
EXPORT_SYMBOL(tty_diag_channel_close);

/* Diag char driver prepares tty driver to receive a write from userspace */
int tty_diag_channel_read(struct legacy_diag_ch *diag_ch,
				struct diag_request *d_req)
{
	d_req_ptr = d_req;

	return 0;
}
EXPORT_SYMBOL(tty_diag_channel_read);

/* Diag char driver has diag packet ready for userspace */
int tty_diag_channel_write(struct legacy_diag_ch *diag_ch,
				struct diag_request *d_req)
{
	struct diag_tty_data *tty_data = diag_ch->priv_channel;
	unsigned char *tty_buf;
	int tty_allocated;

	/* If diag packet is not 1:1 response (perhaps logging packet?),
	   try primary channel */
	if (tty_data == NULL)
		tty_data = &(diag_tty[0]);

	if (tty_data->tty == NULL)
		return -EIO;

	mutex_lock(&diag_tty_lock);

	tty_allocated = tty_prepare_flip_string(tty_data->tty,
						&tty_buf, d_req->length);

	if (tty_allocated < d_req->length) {
		mutex_unlock(&diag_tty_lock);
		return -ENOMEM;
	}

	/* Unset active tty for next request diag tool */
	diag_ch->priv_channel = NULL;

	memcpy(tty_buf, d_req->buf, d_req->length);
	tty_flip_buffer_push(tty_data->tty);

	mutex_unlock(&diag_tty_lock);

	diag_ch->notify(diag_ch->priv, CHANNEL_DIAG_WRITE_DONE, d_req);

	return 0;
}
EXPORT_SYMBOL(tty_diag_channel_write);

void tty_diag_channel_abandon_request()
{
	mutex_lock(&diag_tty_lock);
	legacy_ch.priv_channel = NULL;
	mutex_unlock(&diag_tty_lock);
}
EXPORT_SYMBOL(tty_diag_channel_abandon_request);

static int __init diag_tty_init(void)
{
	int result;

	legacy_ch.notify = NULL;
	legacy_ch.priv = NULL;
	legacy_ch.priv_channel = NULL;

	diag_tty_driver = alloc_tty_driver(DIAG_TTY_MINOR_COUNT);
	if (diag_tty_driver == NULL)
		return -ENOMEM;

	diag_tty_driver->owner = THIS_MODULE;
	diag_tty_driver->driver_name = "tty_diag";
	diag_tty_driver->name = "ttydiag";
	diag_tty_driver->major = DIAG_MAJOR;
	diag_tty_driver->minor_start = 0;
	diag_tty_driver->type = TTY_DRIVER_TYPE_SYSTEM;
	diag_tty_driver->subtype = SYSTEM_TYPE_TTY;
	diag_tty_driver->init_termios = tty_std_termios;
	diag_tty_driver->init_termios.c_cflag = B115200 | CS8 | CREAD;
	diag_tty_driver->init_termios.c_iflag = IGNBRK;
	diag_tty_driver->init_termios.c_oflag = 0;
	diag_tty_driver->init_termios.c_lflag = 0;
	diag_tty_driver->flags = TTY_DRIVER_REAL_RAW | TTY_DRIVER_DYNAMIC_DEV;

	tty_set_operations(diag_tty_driver, &diag_tty_ops);

	result = tty_register_driver(diag_tty_driver);
	if (result) {
		printk(KERN_ERR "Failed to register diag_tty driver.");
		put_tty_driver(diag_tty_driver);
		return result;
	}

	return 0;
}

module_init(diag_tty_init);

MODULE_DESCRIPTION("tty diag driver");