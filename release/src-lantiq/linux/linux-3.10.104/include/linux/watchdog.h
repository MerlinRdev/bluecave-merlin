/*
 *	Generic watchdog defines. Derived from..
 *
 * Berkshire PC Watchdog Defines
 * by Ken Hollis <khollis@bitgate.com>
 *
 */
#ifndef _LINUX_WATCHDOG_H
#define _LINUX_WATCHDOG_H


#include <linux/bitops.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <uapi/linux/watchdog.h>

struct watchdog_ops;
struct watchdog_device;

/** struct watchdog_ops - The watchdog-devices operations
 *
 * @owner:	The module owner.
 * @start:	The routine for starting the watchdog device.
 * @stop:	The routine for stopping the watchdog device.
 * @ping:	The routine that sends a keepalive ping to the watchdog device.
 * @status:	The routine that shows the status of the watchdog device.
 * @set_timeout:The routine for setting the watchdog devices timeout value.
 * @get_timeleft:The routine that get's the time that's left before a reset.
 * @ref:	The ref operation for dyn. allocated watchdog_device structs
 * @unref:	The unref operation for dyn. allocated watchdog_device structs
 * @ioctl:	The routines that handles extra ioctl calls.
 *
 * The watchdog_ops structure contains a list of low-level operations
 * that control a watchdog device. It also contains the module that owns
 * these operations. The start and stop function are mandatory, all other
 * functions are optonal.
 */
struct watchdog_ops {
	struct module *owner;
	/* mandatory operations */
	int (*start)(struct watchdog_device *);
	int (*stop)(struct watchdog_device *);
	/* optional operations */
	int (*ping)(struct watchdog_device *);
	unsigned int (*status)(struct watchdog_device *);
	int (*set_timeout)(struct watchdog_device *, unsigned int);
	unsigned int (*get_timeleft)(struct watchdog_device *);
	void (*ref)(struct watchdog_device *);
	void (*unref)(struct watchdog_device *);
	long (*ioctl)(struct watchdog_device *, unsigned int, unsigned long);
};
/*
 * struct watchdog_core_data - watchdog core internal data
 * @kref:	Reference count.
 * @cdev:	The watchdog's Character device.
 * @wdd:	Pointer to watchdog device.
 * @lock:	Lock for watchdog core.
 * @status:	Watchdog core internal status bits.
 */
struct watchdog_core_data {
	struct kref kref;
	struct cdev cdev;
	struct watchdog_device *wdd;
	struct mutex lock;
	unsigned long last_keepalive;
	unsigned long last_hw_keepalive;
	struct delayed_work work;
	unsigned long status;		/* Internal status bits */
	bool user_call; /* user_space call or wq call for ping */
#define _WDOG_DEV_OPEN		0	/* Opened ? */
#define _WDOG_ALLOW_RELEASE	1	/* Did we receive the magic char ? */
#define _WDOG_KEEPALIVE		2	/* Did we receive a keepalive ? */
};

/** struct watchdog_device - The structure that defines a watchdog device
 *
 * @id:		The watchdog's ID. (Allocated by watchdog_register_device)
 * @cdev:	The watchdog's Character device.
 * @dev:	The device for our watchdog
 * @parent:	The parent bus device
 * @info:	Pointer to a watchdog_info structure.
 * @ops:	Pointer to the list of watchdog operations.
 * @bootstatus:	Status of the watchdog device at boot.
 * @timeout:	The watchdog devices timeout value.
 * @min_timeout:The watchdog devices minimum timeout value.
 * @max_timeout:The watchdog devices maximum timeout value.
 * @driver-data:Pointer to the drivers private data.
 * @lock:	Lock for watchdog core internal use only.
 * @status:	Field that contains the devices internal status bits.
 *
 * The watchdog_device structure contains all information about a
 * watchdog timer device.
 *
 * The driver-data field may not be accessed directly. It must be accessed
 * via the watchdog_set_drvdata and watchdog_get_drvdata helpers.
 *
 * The lock field is for watchdog core internal use only and should not be
 * touched.
 */
struct watchdog_device {
	int id;
	struct cdev cdev;
	struct device *dev;
	struct device *parent;
	const struct watchdog_info *info;
	const struct watchdog_ops *ops;
	unsigned int bootstatus;
	unsigned int timeout;
	unsigned int min_timeout;
	unsigned int max_timeout;
	unsigned int min_hw_heartbeat_ms;
	unsigned int max_hw_heartbeat_ms;
	void *driver_data;
	struct mutex lock;
	struct watchdog_core_data *wd_data;
	unsigned long status;
/* Bit numbers for status flags */
#define WDOG_ACTIVE		0	/* Is the watchdog running/active */
#define WDOG_DEV_OPEN		1	/* Opened via /dev/watchdog ? */
#define WDOG_ALLOW_RELEASE	2	/* Did we receive the magic char ? */
#define WDOG_NO_WAY_OUT		3	/* Is 'nowayout' feature set ? */
#define WDOG_UNREGISTERED	4	/* Has the device been unregistered */
#define WDOG_HW_RUNNING		5	/* True if HW watchdog running */
#define WDOG_ID_NOT_REQ		6	/* watchdog id creation from ida not req */
};

#ifdef CONFIG_WATCHDOG_NOWAYOUT
#define WATCHDOG_NOWAYOUT		1
#define WATCHDOG_NOWAYOUT_INIT_STATUS	(1 << WDOG_NO_WAY_OUT)
#else
#define WATCHDOG_NOWAYOUT		0
#define WATCHDOG_NOWAYOUT_INIT_STATUS	0
#endif

/* Use the following function to check whether or not the watchdog is active */
static inline bool watchdog_active(struct watchdog_device *wdd)
{
	return test_bit(WDOG_ACTIVE, &wdd->status);
}

/*
 * Use the following function to check whether or not the hardware watchdog
 * is running
 */
static inline bool watchdog_hw_running(struct watchdog_device *wdd)
{
	return test_bit(WDOG_HW_RUNNING, &wdd->status);
}


/* Use the following function to set the nowayout feature */
static inline void watchdog_set_nowayout(struct watchdog_device *wdd, bool nowayout)
{
	if (nowayout)
		set_bit(WDOG_NO_WAY_OUT, &wdd->status);
}

/* Use the following function to check if a timeout value is invalid */
static inline bool watchdog_timeout_invalid(struct watchdog_device *wdd, unsigned int t)
{
	/*
	 * The timeout is invalid if
	 * - the requested value is larger than UINT_MAX / 1000
	 *   (since internal calculations are done in milli-seconds),
	 * or
	 * - the requested value is smaller than the configured minimum timeout,
	 * or
	 * - a maximum hardware timeout is not configured, a maximum timeout
	 *   is configured, and the requested value is larger than the
	 *   configured maximum timeout.
	 */
	return t > UINT_MAX / 1000 || t < wdd->min_timeout ||
		(!wdd->max_hw_heartbeat_ms && wdd->max_timeout &&
		 t > wdd->max_timeout);
}

/* Use the following functions to manipulate watchdog driver specific data */
static inline void watchdog_set_drvdata(struct watchdog_device *wdd, void *data)
{
	wdd->driver_data = data;
}

static inline void *watchdog_get_drvdata(struct watchdog_device *wdd)
{
	return wdd->driver_data;
}

/* drivers/watchdog/watchdog_core.c */
extern int watchdog_init_timeout(struct watchdog_device *wdd,
				  unsigned int timeout_parm, struct device *dev);
extern int watchdog_register_device(struct watchdog_device *);
extern void watchdog_unregister_device(struct watchdog_device *);

#endif  /* ifndef _LINUX_WATCHDOG_H */
