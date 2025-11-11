// SPDX-License-Identifier: GPL-2.0

#include <asm/unaligned.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-event.h>
#include <media/v4l2-device.h>
#include <media/v4l2-fwnode.h>
#include <media/v4l2-mediabus.h>
#include <media/v4l2-subdev.h>

#define PIXEL_RATE					180000000ULL

#define AR0234_REG_VALUE_16BIT		2

/* Chip ID */
#define AR0234_REG_CHIP_ID			0x3000	//16-bit
#define AR0234_CHIP_ID				0x0a56

#define AR0234_REG_MODE_SELECT		0x301a	//16-bit
#define AR0234_MODE_RESET			0x00d9
#define AR0234_MODE_STANDBY			0x2058
#define AR0234_MODE_STREAMING		0x205c

/* V_TIMING internal */
#define AR0234_REG_VTS				0x300a	//16-bit
#define AR0234_VTS_MAX				0xffff

/* Exposure control */
#define AR0234_REG_EXPOSURE			0x3012	//16-bit
#define AR0234_EXPOSURE_MIN			0
#define AR0234_EXPOSURE_MAX_MARGIN	80
#define AR0234_EXPOSURE_STEP		1

/* Analog gain control */
#define AR0234_REG_ANALOG_GAIN		0x3060	//16-bit
#define AR0234_ANAL_GAIN_MIN		0
#define AR0234_ANAL_GAIN_MAX		0x7f
#define AR0234_ANAL_GAIN_STEP		1
#define AR0234_ANAL_GAIN_DEFAULT	0xe

/* Digital gain control */
#define AR0234_REG_GLOBAL_GAIN		0x305e	//16-bit
#define AR0234_DGTL_GAIN_MIN		0
#define AR0234_DGTL_GAIN_MAX		0x7ff
#define AR0234_DGTL_GAIN_STEP		1
#define AR0234_DGTL_GAIN_DEFAULT	0x80

/* AR0234 native and active pixel array size. */
#define AR0234_NATIVE_WIDTH			1200U
#define AR0234_NATIVE_HEIGHT		1920U
#define AR0234_PIXEL_ARRAY_LEFT		0		//border of "dark pixels"
#define AR0234_PIXEL_ARRAY_TOP		0		//border of "dark pixels"
#define AR0234_PIXEL_ARRAY_WIDTH	1200U
#define AR0234_PIXEL_ARRAY_HEIGHT	1920U


struct ar0234_reg {
	u16 address;
	u16 val;
};

struct ar0234_reg_list {
	unsigned int num_of_regs;
	const struct ar0234_reg *regs;
};

struct ar0234_link_freq_config {
	const struct ar0234_reg_list reg_list;
};

struct ar0234_mode {
	/* Frame width */
	unsigned int width;
	/* Frame height */
	unsigned int height;

	/* Analog crop rectangle. */
	struct v4l2_rect crop;

	u32 vts_def;
	u32 vts_min;

	u32 link_freq_index;
	u32 code;

	u32 hblank;
	u32 vblank;

	/* Sensor register settings for this mode */
	const struct ar0234_reg_list reg_list;
};

static const struct ar0234_reg link_freq_360M_1280x960_10bit_2lane[] = {
	{0x302a, 0x0005},
	{0x302c, 0x0004},
	{0x302e, 0x0003},
	{0x3030, 0x0050},
	{0x3036, 0x000a},
	{0x3038, 0x0002},
	{0x31b0, 0x006e},
	{0x31b2, 0x0050},
	{0x31b4, 0x4207},
	{0x31b6, 0x2213},
	{0x31b8, 0x704a},
	{0x31ba, 0x0289},
	{0x31bc, 0x8c08},
	{0x31ae, 0x0202},
	{0x3002, 0x0080},
	{0x3004, 0x0148},
	{0x3006, 0x043f},
	{0x3008, 0x0647},
	{0x300a, 0x05b5},
	{0x300c, 0x0268},
	{0x3012, 0x058c},
	{0x31ac, 0x0a0a},
	{0x306e, 0x9010},
	{0x30a2, 0x0001},
	{0x30a6, 0x0001},
	{0x3082, 0x0003},
	{0x3040, 0x0000},
	{0x31d0, 0x0000},
};

static const struct ar0234_reg mode_1280x960_10bit_2lane[] = {
	{0x3f4c, 0x121f},
	{0x3f4e, 0x121f},
	{0x3f50, 0x0b81},
	{0x31e0, 0x0003},
	{0x30b0, 0x0028},
	{0x3088, 0x8000},
	{0x3086, 0xc1ae},
	{0x3086, 0x327f},
	{0x3086, 0x5780},
	{0x3086, 0x272f},
	{0x3086, 0x7416},
	{0x3086, 0x7e13},
	{0x3086, 0x8000},
	{0x3086, 0x307e},
	{0x3086, 0xff80},
	{0x3086, 0x20c3},
	{0x3086, 0xb00e},
	{0x3086, 0x8190},
	{0x3086, 0x1643},
	{0x3086, 0x1651},
	{0x3086, 0x9d3e},
	{0x3086, 0x9545},
	{0x3086, 0x2209},
	{0x3086, 0x3781},
	{0x3086, 0x9016},
	{0x3086, 0x4316},
	{0x3086, 0x7f90},
	{0x3086, 0x8000},
	{0x3086, 0x387f},
	{0x3086, 0x1380},
	{0x3086, 0x233b},
	{0x3086, 0x7f93},
	{0x3086, 0x4502},
	{0x3086, 0x8000},
	{0x3086, 0x7fb0},
	{0x3086, 0x8d66},
	{0x3086, 0x7f90},
	{0x3086, 0x8192},
	{0x3086, 0x3c16},
	{0x3086, 0x357f},
	{0x3086, 0x9345},
	{0x3086, 0x0280},
	{0x3086, 0x007f},
	{0x3086, 0xb08d},
	{0x3086, 0x667f},
	{0x3086, 0x9081},
	{0x3086, 0x8237},
	{0x3086, 0x4502},
	{0x3086, 0x3681},
	{0x3086, 0x8044},
	{0x3086, 0x1631},
	{0x3086, 0x4374},
	{0x3086, 0x1678},
	{0x3086, 0x7b7d},
	{0x3086, 0x4502},
	{0x3086, 0x450a},
	{0x3086, 0x7e12},
	{0x3086, 0x8180},
	{0x3086, 0x377f},
	{0x3086, 0x1045},
	{0x3086, 0x0a0e},
	{0x3086, 0x7fd4},
	{0x3086, 0x8024},
	{0x3086, 0x0e82},
	{0x3086, 0x9cc2},
	{0x3086, 0xafa8},
	{0x3086, 0xaa03},
	{0x3086, 0x430d},
	{0x3086, 0x2d46},
	{0x3086, 0x4316},
	{0x3086, 0x5f16},
	{0x3086, 0x530d},
	{0x3086, 0x1660},
	{0x3086, 0x401e},
	{0x3086, 0x2904},
	{0x3086, 0x2984},
	{0x3086, 0x81e7},
	{0x3086, 0x816f},
	{0x3086, 0x1706},
	{0x3086, 0x81e7},
	{0x3086, 0x7f81},
	{0x3086, 0x5c0d},
	{0x3086, 0x5754},
	{0x3086, 0x495f},
	{0x3086, 0x5305},
	{0x3086, 0x5307},
	{0x3086, 0x4d2b},
	{0x3086, 0xf810},
	{0x3086, 0x164c},
	{0x3086, 0x0755},
	{0x3086, 0x562b},
	{0x3086, 0xb82b},
	{0x3086, 0x984e},
	{0x3086, 0x1129},
	{0x3086, 0x9460},
	{0x3086, 0x5c09},
	{0x3086, 0x5c1b},
	{0x3086, 0x4002},
	{0x3086, 0x4500},
	{0x3086, 0x4580},
	{0x3086, 0x29b6},
	{0x3086, 0x7f80},
	{0x3086, 0x4004},
	{0x3086, 0x7f88},
	{0x3086, 0x4109},
	{0x3086, 0x5c0b},
	{0x3086, 0x29b2},
	{0x3086, 0x4115},
	{0x3086, 0x5c03},
	{0x3086, 0x4105},
	{0x3086, 0x5f2b},
	{0x3086, 0x902b},
	{0x3086, 0x8081},
	{0x3086, 0x6f40},
	{0x3086, 0x1041},
	{0x3086, 0x0160},
	{0x3086, 0x29a2},
	{0x3086, 0x29a3},
	{0x3086, 0x5f4d},
	{0x3086, 0x1c17},
	{0x3086, 0x0281},
	{0x3086, 0xe729},
	{0x3086, 0x8345},
	{0x3086, 0x8840},
	{0x3086, 0x0f7f},
	{0x3086, 0x8a40},
	{0x3086, 0x2345},
	{0x3086, 0x8024},
	{0x3086, 0x4008},
	{0x3086, 0x7f88},
	{0x3086, 0x5d29},
	{0x3086, 0x9288},
	{0x3086, 0x102b},
	{0x3086, 0x0489},
	{0x3086, 0x165c},
	{0x3086, 0x4386},
	{0x3086, 0x170b},
	{0x3086, 0x5c03},
	{0x3086, 0x8a48},
	{0x3086, 0x4d4e},
	{0x3086, 0x2b80},
	{0x3086, 0x4c09},
	{0x3086, 0x4119},
	{0x3086, 0x816f},
	{0x3086, 0x4110},
	{0x3086, 0x4001},
	{0x3086, 0x6029},
	{0x3086, 0x8229},
	{0x3086, 0x8329},
	{0x3086, 0x435c},
	{0x3086, 0x055f},
	{0x3086, 0x4d1c},
	{0x3086, 0x81e7},
	{0x3086, 0x4502},
	{0x3086, 0x8180},
	{0x3086, 0x7f80},
	{0x3086, 0x410a},
	{0x3086, 0x9144},
	{0x3086, 0x1609},
	{0x3086, 0x2fc3},
	{0x3086, 0xb130},
	{0x3086, 0xc3b1},
	{0x3086, 0x0343},
	{0x3086, 0x164a},
	{0x3086, 0x0a43},
	{0x3086, 0x160b},
	{0x3086, 0x4316},
	{0x3086, 0x8f43},
	{0x3086, 0x1690},
	{0x3086, 0x4316},
	{0x3086, 0x7f81},
	{0x3086, 0x450a},
	{0x3086, 0x410f},
	{0x3086, 0x7f83},
	{0x3086, 0x5d29},
	{0x3086, 0x4488},
	{0x3086, 0x102b},
	{0x3086, 0x0453},
	{0x3086, 0x0d40},
	{0x3086, 0x2345},
	{0x3086, 0x0240},
	{0x3086, 0x087f},
	{0x3086, 0x8053},
	{0x3086, 0x0d89},
	{0x3086, 0x165c},
	{0x3086, 0x4586},
	{0x3086, 0x170b},
	{0x3086, 0x5c05},
	{0x3086, 0x8a60},
	{0x3086, 0x4b91},
	{0x3086, 0x4416},
	{0x3086, 0x09c1},
	{0x3086, 0x2ca9},
	{0x3086, 0xab30},
	{0x3086, 0x51b3},
	{0x3086, 0x3d5a},
	{0x3086, 0x7e3d},
	{0x3086, 0x7e19},
	{0x3086, 0x8000},
	{0x3086, 0x8b1f},
	{0x3086, 0x2a1f},
	{0x3086, 0x83a2},
	{0x3086, 0x7516},
	{0x3086, 0xad33},
	{0x3086, 0x450a},
	{0x3086, 0x7f53},
	{0x3086, 0x8023},
	{0x3086, 0x8c66},
	{0x3086, 0x7f13},
	{0x3086, 0x8184},
	{0x3086, 0x1481},
	{0x3086, 0x8031},
	{0x3086, 0x3d64},
	{0x3086, 0x452a},
	{0x3086, 0x9451},
	{0x3086, 0x9e96},
	{0x3086, 0x3d2b},
	{0x3086, 0x3d1b},
	{0x3086, 0x529f},
	{0x3086, 0x0e3d},
	{0x3086, 0x083d},
	{0x3086, 0x167e},
	{0x3086, 0x307e},
	{0x3086, 0x1175},
	{0x3086, 0x163e},
	{0x3086, 0x970e},
	{0x3086, 0x82b2},
	{0x3086, 0x3d7f},
	{0x3086, 0xac3e},
	{0x3086, 0x4502},
	{0x3086, 0x7e11},
	{0x3086, 0x7fd0},
	{0x3086, 0x8000},
	{0x3086, 0x8c66},
	{0x3086, 0x7f90},
	{0x3086, 0x8194},
	{0x3086, 0x3f44},
	{0x3086, 0x1681},
	{0x3086, 0x8416},
	{0x3086, 0x2c2c},
	{0x3086, 0x2c2c},
	{0x302a, 0x0005},
	{0x302c, 0x0001},
	{0x302e, 0x0003},
	{0x3030, 0x0032},
	{0x3036, 0x000a},
	{0x3038, 0x0001},
	{0x30b0, 0x0028},
	{0x31b0, 0x0082},
	{0x31b2, 0x005c},
	{0x31b4, 0x5248},
	{0x31b6, 0x3257},
	{0x31b8, 0x904b},
	{0x31ba, 0x030b},
	{0x31bc, 0x8e09},
	{0x3354, 0x002b},
	{0x31d0, 0x0000},
	{0x31ae, 0x0204},
	{0x3002, 0x00d0},
	{0x3004, 0x0148},
	{0x3006, 0x048f},
	{0x3008, 0x0647},
	{0x3064, 0x1802},
	{0x300a, 0x04c4},
	{0x300c, 0x04c4},
	{0x30a2, 0x0001},
	{0x30a6, 0x0001},
	{0x3012, 0x010c},
	{0x3786, 0x0006},
	{0x31ae, 0x0202},
	{0x3088, 0x8050},
	{0x3086, 0x9237},
	{0x3044, 0x0410},
	{0x3094, 0x03d4},
	{0x3096, 0x0280},
	{0x30ba, 0x7606},
	{0x30b0, 0x0028},
	{0x30ba, 0x7600},
	{0x30fe, 0x002a},
	{0x31de, 0x0410},
	{0x3ed6, 0x1435},
	{0x3ed8, 0x9865},
	{0x3eda, 0x7698},
	{0x3edc, 0x99ff},
	{0x3ee2, 0xbb88},
	{0x3ee4, 0x8836},
	{0x3ef0, 0x1cf0},
	{0x3ef2, 0x0000},
	{0x3ef8, 0x6166},
	{0x3efa, 0x3333},
	{0x3efc, 0x6634},
	{0x3088, 0x81ba},
	{0x3086, 0x3d02},
	{0x3276, 0x05dc},
	{0x3f00, 0x9d05},
	{0x3ed2, 0xfa86},
	{0x3eee, 0xa4fe},
	{0x3ecc, 0x6e42},
	{0x3ecc, 0x0e42},
	{0x3eec, 0x0c0c},
	{0x3ee8, 0xaae4},
	{0x3ee6, 0x3363},
	{0x3ee6, 0x3363},
	{0x3ee8, 0xaae4},
	{0x3ee8, 0xaae4},
	{0x3180, 0xc24f},
	{0x3102, 0x5000},
	{0x3060, 0x000d},
	{0x3ed0, 0xff44},
	{0x3ed2, 0xaa86},
	{0x3ed4, 0x031f},
	{0x3eee, 0xa4aa},
};

#define AR0234_LINK_FREQ_360MHZ		360000000ULL

static const s64 link_freq_menu_items[] = {
	AR0234_LINK_FREQ_360MHZ
};

static const struct ar0234_link_freq_config link_freq_configs[] = {
	{
		.reg_list = {
			.num_of_regs =
				ARRAY_SIZE(link_freq_360M_1280x960_10bit_2lane),
			.regs = link_freq_360M_1280x960_10bit_2lane,
		}
	},
};

static const struct ar0234_mode supported_modes[] = {
	{
		.width = AR0234_NATIVE_WIDTH,
		.height = AR0234_NATIVE_HEIGHT,
		.crop = {
			.left = AR0234_PIXEL_ARRAY_LEFT,
			.top = AR0234_PIXEL_ARRAY_TOP,
			.width = AR0234_NATIVE_WIDTH,
			.height = AR0234_NATIVE_HEIGHT
		},
		.vts_def = 2435,
		.vts_min = 2435,
		.code = MEDIA_BUS_FMT_SGRBG10_1X10,
		.reg_list = {
			.num_of_regs = ARRAY_SIZE(mode_1280x960_10bit_2lane),
			.regs = mode_1280x960_10bit_2lane,
		},
		.link_freq_index = 0,
		.hblank = 3600,
		.vblank = 1475,
	},
};

struct ar0234 {
	struct v4l2_subdev sd;
	struct media_pad pad;

	struct v4l2_mbus_framefmt fmt;
	struct v4l2_ctrl_handler ctrl_handler;

	/* V4L2 Controls */
	struct v4l2_ctrl *link_freq;
	struct v4l2_ctrl *exposure;
	struct v4l2_ctrl *analogue_gain;
	struct v4l2_ctrl *digital_gain;
	struct v4l2_ctrl *pixel_rate;
	struct v4l2_ctrl *hblank;
	struct v4l2_ctrl *vblank;

	//struct regmap *regmap;
	unsigned long link_freq_bitmap;

	/* Current mode */
	const struct ar0234_mode *mode;

	/*
	 * Mutex for serialized access:
	 * Protect sensor module set pad format and start/stop streaming safely.
	 */
	struct mutex mutex;

	/* Streaming on/off */
	bool streaming;

	/* GPIOs for power control of VLS GM2 AR0234 Camera*/
	struct gpio_desc *power_gpio;
	struct gpio_desc *reset_gpio;
	
};

static inline struct ar0234 *to_ar0234(struct v4l2_subdev *_sd)
{
	return container_of(_sd, struct ar0234, sd);
}

static int ar0234_power_on(struct device *dev)
{
	struct v4l2_subdev *sd = dev_get_drvdata(dev);
	struct ar0234 *ar0234 = to_ar0234(sd);

	dev_dbg(dev, "Turning on power\n");

	// Set power enable (pin 7) HIGH to provide power to the sensor.
	gpiod_set_value_cansleep(ar0234->power_gpio, 1);

	usleep_range(20000, 25000); //20 ms sleep

	// Set reset (pin 4) HIGH to take the sensor out of reset.
	gpiod_set_value_cansleep(ar0234->reset_gpio, 1);

	usleep_range(20000, 25000); //why not use msleep?

	return 0;
}

static int ar0234_power_off(struct device *dev)
{
	struct v4l2_subdev *sd = dev_get_drvdata(dev);
	struct ar0234 *ar0234 = to_ar0234(sd);

	dev_dbg(dev, "Turning off power\n");

	// Put the sensor into reset first (pin 4 LOW).
	gpiod_set_value_cansleep(ar0234->reset_gpio, 0);

	// Then disable the main power supply (pin 7 LOW).
	gpiod_set_value_cansleep(ar0234->power_gpio, 0);

	return 0;
}

/* Reads registers that is only 16-bit */
static int ar0234_read_reg(struct ar0234 *ar0234, u16 reg, u16 *val)
{
	struct i2c_client *client = v4l2_get_subdevdata(&ar0234->sd);
	struct i2c_msg msgs[2];
	u8 addr_buf[2] = { reg >> 8, reg & 0xff };
	u8 data_buf[2] = { 0 };
	int ret;

	msgs[0].addr = client->addr;
	msgs[0].flags = 0;
	msgs[0].len = ARRAY_SIZE(addr_buf);
	msgs[0].buf = addr_buf;

	msgs[1].addr = client->addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = AR0234_REG_VALUE_16BIT;
	msgs[1].buf = data_buf;

	ret = i2c_transfer(client->adapter, msgs, ARRAY_SIZE(msgs));
	if (ret != ARRAY_SIZE(msgs))
		return -EIO;

	*val = get_unaligned_be16(data_buf);

	return 0;
}
/* Always sends 16 bit data to the 16 bit registers */
static int ar0234_write_reg(struct ar0234 *ar0234, u16 reg, u16 val)
{
	struct i2c_client *client = v4l2_get_subdevdata(&ar0234->sd);
	u8 buf[4];

	put_unaligned_be16(reg, buf);
	put_unaligned_be16(val, buf + 2);

	if (i2c_master_send(client, buf, sizeof(buf)) != sizeof(buf))
		return -EIO;

	return 0;
}
/* Write a list of registers as 16 bit register + 16 bit data */
static int ar0234_write_regs(struct ar0234 *ar0234,
			     const struct ar0234_reg *regs, u32 len)
{
	struct i2c_client *client = v4l2_get_subdevdata(&ar0234->sd);
	unsigned int i;
	int ret;

	for (i = 0; i < len; i++) {
		ret = ar0234_write_reg(ar0234, regs[i].address, regs[i].val);
		if (ret) {
			dev_err_ratelimited(&client->dev,
					    "Failed to write reg 0x%4.4x. error = %d\n",
					    regs[i].address, ret);
			return ret;
		}
	}
	return 0;
}

static void ar0234_set_default_format(struct ar0234 *ar0234)
{
	struct v4l2_mbus_framefmt *fmt;

	fmt = &ar0234->fmt;
	fmt->code = MEDIA_BUS_FMT_SGRBG10_1X10;
	fmt->colorspace = V4L2_COLORSPACE_SRGB;
	fmt->ycbcr_enc = V4L2_MAP_YCBCR_ENC_DEFAULT(fmt->colorspace);
	fmt->quantization = V4L2_MAP_QUANTIZATION_DEFAULT(true,
							  fmt->colorspace,
							  fmt->ycbcr_enc);
	fmt->xfer_func = V4L2_MAP_XFER_FUNC_DEFAULT(fmt->colorspace);
	fmt->width = supported_modes[0].width;
	fmt->height = supported_modes[0].height;
	fmt->field = V4L2_FIELD_NONE;
}

static int ar0234_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct ar0234 *ar0234 = to_ar0234(sd);
	struct v4l2_mbus_framefmt *try_fmt =
		v4l2_subdev_get_try_format(sd, fh->pad, 0);
	struct v4l2_rect *try_crop;

	mutex_lock(&ar0234->mutex);

	/* Initialize try_fmt */
	try_fmt->width = supported_modes[0].width;
	try_fmt->height = supported_modes[0].height;
	try_fmt->code = MEDIA_BUS_FMT_SGRBG10_1X10;
	try_fmt->field = V4L2_FIELD_NONE;

	/* Initialize try_crop rectangle. */
	try_crop = v4l2_subdev_get_try_crop(sd, fh->pad, 0);
	try_crop->top = AR0234_PIXEL_ARRAY_TOP;
	try_crop->left = AR0234_PIXEL_ARRAY_LEFT;
	try_crop->width = AR0234_PIXEL_ARRAY_WIDTH;
	try_crop->height = AR0234_PIXEL_ARRAY_HEIGHT;

	mutex_unlock(&ar0234->mutex);

	return 0;
}

static int ar0234_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct ar0234 *ar0234 =
		container_of(ctrl->handler, struct ar0234, ctrl_handler);
	struct i2c_client *client = v4l2_get_subdevdata(&ar0234->sd);
	int ret;

	if (ctrl->id == V4L2_CID_VBLANK) {
		int exposure_max, exposure_def;

		/* Update max exposure while meeting expected vblanking */
		exposure_max = ar0234->mode->height + ctrl->val -
			       AR0234_EXPOSURE_MAX_MARGIN;
		exposure_def = ar0234->mode->height - AR0234_EXPOSURE_MAX_MARGIN;
		__v4l2_ctrl_modify_range(ar0234->exposure,
					 ar0234->exposure->minimum,
					 exposure_max,
					 ar0234->exposure->step,
					 exposure_def);
	}

	/* V4L2 controls values will be applied only when power is already up */
	if (!pm_runtime_get_if_in_use(&client->dev))
		return 0;

	switch (ctrl->id) {
	case V4L2_CID_ANALOGUE_GAIN:
		ret = ar0234_write_reg(ar0234, AR0234_REG_ANALOG_GAIN, ctrl->val);
		break;

	case V4L2_CID_DIGITAL_GAIN:
		ret = ar0234_write_reg(ar0234, AR0234_REG_GLOBAL_GAIN, ctrl->val);
		break;

	case V4L2_CID_EXPOSURE:
		ret = ar0234_write_reg(ar0234, AR0234_REG_EXPOSURE, ctrl->val);
		break;

	case V4L2_CID_VBLANK:
		ret = ar0234_write_reg(ar0234, AR0234_REG_VTS,
				ar0234->mode->height + ctrl->val);
		break;

	default:
		dev_info(&client->dev,
			 "ctrl(id:0x%x,val:0x%x) is not handled\n",
			 ctrl->id, ctrl->val);
		ret = -EINVAL;
		break;
	}

	pm_runtime_put(&client->dev);

	return ret;
}

static const struct v4l2_ctrl_ops ar0234_ctrl_ops = {
	.s_ctrl = ar0234_set_ctrl,
};
/* Pay Attention */
static int ar0234_init_controls(struct ar0234 *ar0234)
{
	struct i2c_client *client = v4l2_get_subdevdata(&ar0234->sd);
	struct v4l2_fwnode_device_properties props;
	struct v4l2_ctrl_handler *ctrl_hdlr;
	s64 exposure_max, vblank, hblank;
	u32 link_freq_size;
	int ret;

	ctrl_hdlr = &ar0234->ctrl_handler;
	ret = v4l2_ctrl_handler_init(ctrl_hdlr, 8);
	if (ret)
		return ret;

	mutex_init(&ar0234->mutex);
	ctrl_hdlr->lock = &ar0234->mutex;

	link_freq_size = ARRAY_SIZE(link_freq_menu_items) - 1;
	ar0234->link_freq = v4l2_ctrl_new_int_menu(ctrl_hdlr,
						   &ar0234_ctrl_ops,
						   V4L2_CID_LINK_FREQ,
						   link_freq_size, 0,
						   link_freq_menu_items);
	if (ar0234->link_freq)
		ar0234->link_freq->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	v4l2_ctrl_new_std(ctrl_hdlr, &ar0234_ctrl_ops, V4L2_CID_ANALOGUE_GAIN,
			  AR0234_ANAL_GAIN_MIN, AR0234_ANAL_GAIN_MAX,
			  AR0234_ANAL_GAIN_STEP, AR0234_ANAL_GAIN_DEFAULT);
	v4l2_ctrl_new_std(ctrl_hdlr, &ar0234_ctrl_ops, V4L2_CID_DIGITAL_GAIN,
			  AR0234_DGTL_GAIN_MIN, AR0234_DGTL_GAIN_MAX,
			  AR0234_DGTL_GAIN_STEP, AR0234_DGTL_GAIN_DEFAULT);

	exposure_max = ar0234->mode->vts_def - AR0234_EXPOSURE_MAX_MARGIN;
	ar0234->exposure = v4l2_ctrl_new_std(ctrl_hdlr, &ar0234_ctrl_ops,
					     V4L2_CID_EXPOSURE,
					     AR0234_EXPOSURE_MIN, exposure_max,
					     AR0234_EXPOSURE_STEP,
					     exposure_max);

	ar0234->pixel_rate = v4l2_ctrl_new_std(ctrl_hdlr, &ar0234_ctrl_ops,
					       V4L2_CID_PIXEL_RATE,
					       PIXEL_RATE,
					       PIXEL_RATE, 1,
					       PIXEL_RATE);
	if (ar0234->pixel_rate)
		ar0234->pixel_rate->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	vblank = AR0234_VTS_MAX - ar0234->mode->height;
	ar0234->vblank = v4l2_ctrl_new_std(ctrl_hdlr, &ar0234_ctrl_ops,
					   V4L2_CID_VBLANK, 0, vblank, 1,
					   ar0234->mode->vblank);
	hblank = ar0234->mode->hblank;
	ar0234->hblank = v4l2_ctrl_new_std(ctrl_hdlr, &ar0234_ctrl_ops,
					   V4L2_CID_HBLANK, hblank, hblank, 1,
					   hblank);
	if (ar0234->hblank)
		ar0234->hblank->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	if (ctrl_hdlr->error)
		return ctrl_hdlr->error;

	ret = v4l2_fwnode_device_parse(&client->dev, &props);
	if (ret)
		return ret;

	ret = v4l2_ctrl_new_fwnode_properties(ctrl_hdlr, &ar0234_ctrl_ops,
					      &props);
	if (ret)
		return ret;

	ar0234->sd.ctrl_handler = ctrl_hdlr;

	return 0;
}

static void ar0234_reset_colorspace(struct v4l2_mbus_framefmt *fmt)
{
	fmt->colorspace = V4L2_COLORSPACE_SRGB;
	fmt->ycbcr_enc = V4L2_MAP_YCBCR_ENC_DEFAULT(fmt->colorspace);
	fmt->quantization = V4L2_MAP_QUANTIZATION_DEFAULT(true,
							  fmt->colorspace,
							  fmt->ycbcr_enc);
	fmt->xfer_func = V4L2_MAP_XFER_FUNC_DEFAULT(fmt->colorspace);
}

static void ar0234_update_pad_format(struct ar0234 *ar0234,
				     const struct ar0234_mode *mode,
				     struct v4l2_subdev_format *fmt)
{
	fmt->format.width = mode->width;
	fmt->format.height = mode->height;
	fmt->format.field = V4L2_FIELD_NONE;
	ar0234_reset_colorspace(&fmt->format);
}

static int __ar0234_get_pad_format(struct ar0234 *ar0234,
				   struct v4l2_subdev_pad_config *cfg,
				   struct v4l2_subdev_format *fmt)
{
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
		struct v4l2_mbus_framefmt *try_fmt =
			v4l2_subdev_get_try_format(&ar0234->sd, cfg, fmt->pad);
		/* update the code */
		try_fmt->code = MEDIA_BUS_FMT_SGRBG10_1X10;
		fmt->format = *try_fmt;
	} else {
		ar0234_update_pad_format(ar0234, ar0234->mode, fmt);
		fmt->format.code = MEDIA_BUS_FMT_SGRBG10_1X10;
	}

	return 0;
}

static int ar0234_get_pad_format(struct v4l2_subdev *sd,
				 struct v4l2_subdev_pad_config *cfg,
				 struct v4l2_subdev_format *fmt)
{
	struct ar0234 *ar0234 = to_ar0234(sd);
	int ret;

	mutex_lock(&ar0234->mutex);
	ret = __ar0234_get_pad_format(ar0234, cfg, fmt);
	mutex_unlock(&ar0234->mutex);

	return ret;
}

static int ar0234_start_streaming(struct ar0234 *ar0234)
{
	struct i2c_client *client = v4l2_get_subdevdata(&ar0234->sd);
	const struct ar0234_reg_list *reg_list;
	int link_freq_index, ret;

	ret = pm_runtime_get_sync(&client->dev);
	if (ret < 0) {
		pm_runtime_put_noidle(&client->dev);
		return ret;
	}

	ret = ar0234_write_reg(ar0234, AR0234_REG_MODE_SELECT, AR0234_MODE_RESET);
	
	if (ret) {
		dev_err(&client->dev, "failed to reset");
		goto err_rpm_put;
	}

	usleep_range(1000, 1500);

	/* Apply default values of current mode */
	reg_list = &ar0234->mode->reg_list;
	ret = ar0234_write_regs(ar0234, reg_list->regs, reg_list->num_of_regs);
	if (ret) {
		dev_err(&client->dev, "failed to set mode");
		goto err_rpm_put;
	}

	link_freq_index = ar0234->mode->link_freq_index;
	if (link_freq_index > 0) {
		reg_list = &link_freq_configs[link_freq_index].reg_list;
		ret = ar0234_write_regs(ar0234, reg_list->regs, reg_list->num_of_regs);
		if (ret) {
			dev_err(&client->dev, "failed to set plls");
			goto err_rpm_put;
		}
	}

	/* Apply customized values from user */
	ret = __v4l2_ctrl_handler_setup(ar0234->sd.ctrl_handler);
	if (ret)
		goto err_rpm_put;

	/* set stream on register */
	ret = ar0234_write_reg(ar0234, AR0234_REG_MODE_SELECT, AR0234_MODE_STREAMING);
	if (ret) {
		dev_err(&client->dev, "failed to start stream");
		goto err_rpm_put;
	}

	return 0;

err_rpm_put:
	pm_runtime_put(&client->dev);
	return ret;
}

static int ar0234_stop_streaming(struct ar0234 *ar0234)
{
	int ret;
	struct i2c_client *client = v4l2_get_subdevdata(&ar0234->sd);

	/* set stream off register */
	ret = ar0234_write_reg(ar0234, AR0234_REG_MODE_SELECT, AR0234_MODE_STANDBY);

	if (ret < 0)
		dev_err(&client->dev, "failed to stop stream");

	pm_runtime_put(&client->dev);
	return ret;
}

static int ar0234_set_stream(struct v4l2_subdev *sd, int enable)
{
	struct ar0234 *ar0234 = to_ar0234(sd);
	int ret = 0;

	mutex_lock(&ar0234->mutex);
	if (ar0234->streaming == enable) {
		mutex_unlock(&ar0234->mutex);
		return 0;
	}

	if (enable) {
		/*
		 * Apply default & customized values
		 * and then start streaming.
		 */
		ret = ar0234_start_streaming(ar0234);
		if (ret)
			goto err_unlock;
	} else {
		ar0234_stop_streaming(ar0234);
	}

	ar0234->streaming = enable;

	mutex_unlock(&ar0234->mutex);

	return ret;

err_unlock:
	mutex_unlock(&ar0234->mutex);

	return ret;
}
/* Pay Attention */
static int ar0234_set_pad_format(struct v4l2_subdev *sd,
				 struct v4l2_subdev_pad_config *cfg,
				 struct v4l2_subdev_format *fmt)
{
	struct ar0234 *ar0234 = to_ar0234(sd);
	const struct ar0234_mode *mode;
	struct v4l2_mbus_framefmt *framefmt;
	s64 hblank;

	mutex_lock(&ar0234->mutex);

	mode = v4l2_find_nearest_size(supported_modes,
				      ARRAY_SIZE(supported_modes),
				      width, height,
				      fmt->format.width, fmt->format.height);

	ar0234_update_pad_format(ar0234, mode, fmt);
	fmt->format.code = MEDIA_BUS_FMT_SGRBG10_1X10;

	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
		framefmt = v4l2_subdev_get_try_format(sd, cfg, fmt->pad);
		*framefmt = fmt->format;
	} else {
		ar0234->fmt = fmt->format;
		ar0234->mode = mode;

		__v4l2_ctrl_s_ctrl(ar0234->link_freq, mode->link_freq_index);

		hblank = ar0234->mode->hblank;
		__v4l2_ctrl_modify_range(ar0234->hblank, hblank, hblank,
					 1, hblank);

		__v4l2_ctrl_modify_range(ar0234->vblank, 0,
					 AR0234_VTS_MAX - mode->height, 1,
					 ar0234->mode->vblank);
		__v4l2_ctrl_s_ctrl(ar0234->vblank, ar0234->mode->vblank);
	}

	mutex_unlock(&ar0234->mutex);

	return 0;
}

static int ar0234_enum_mbus_code(struct v4l2_subdev *sd,
				 struct v4l2_subdev_pad_config *cfg,
				 struct v4l2_subdev_mbus_code_enum *code)
{
	if (code->index > 0)
		return -EINVAL;

	code->code = MEDIA_BUS_FMT_SGRBG10_1X10;

	return 0;
}

static int ar0234_enum_frame_size(struct v4l2_subdev *sd,
				  struct v4l2_subdev_pad_config *cfg,
				  struct v4l2_subdev_frame_size_enum *fse)
{
	if (fse->index >= ARRAY_SIZE(supported_modes))
		return -EINVAL;

	if (fse->code != MEDIA_BUS_FMT_SGRBG10_1X10)
		return -EINVAL;

	fse->min_width = supported_modes[fse->index].width;
	fse->max_width = fse->min_width;
	fse->min_height = supported_modes[fse->index].height;
	fse->max_height = fse->min_height;

	return 0;
}

static const struct v4l2_rect *
__ar0234_get_pad_crop(struct ar0234 *ar0234, struct v4l2_subdev_pad_config *cfg,
		      unsigned int pad, enum v4l2_subdev_format_whence which)
{
	switch (which) {
	case V4L2_SUBDEV_FORMAT_TRY:
		return v4l2_subdev_get_try_crop(&ar0234->sd, cfg, pad);
	case V4L2_SUBDEV_FORMAT_ACTIVE:
		return &ar0234->mode->crop;
	}

	return NULL;
}

static int ar0234_get_selection(struct v4l2_subdev *sd,
				struct v4l2_subdev_pad_config *cfg,
				struct v4l2_subdev_selection *sel)
{
	switch (sel->target) {
	case V4L2_SEL_TGT_CROP: {
		struct ar0234 *ar0234 = to_ar0234(sd);

		mutex_lock(&ar0234->mutex);
		sel->r = *__ar0234_get_pad_crop(ar0234, cfg, sel->pad,
						sel->which);
		mutex_unlock(&ar0234->mutex);

		return 0;
	}

	case V4L2_SEL_TGT_CROP_DEFAULT:
	case V4L2_SEL_TGT_NATIVE_SIZE:
	case V4L2_SEL_TGT_CROP_BOUNDS:
		sel->r.top = AR0234_PIXEL_ARRAY_TOP;
		sel->r.left = AR0234_PIXEL_ARRAY_LEFT;
		sel->r.width = AR0234_NATIVE_WIDTH;
		sel->r.height = AR0234_NATIVE_HEIGHT;
		break;

		return 0;
	}

	return -EINVAL;
}

static const struct v4l2_subdev_video_ops ar0234_video_ops = {
	.s_stream = ar0234_set_stream,
};

static const struct v4l2_subdev_pad_ops ar0234_pad_ops = {
	.set_fmt = ar0234_set_pad_format,
	.get_fmt = ar0234_get_pad_format,
	.enum_mbus_code = ar0234_enum_mbus_code,
	.enum_frame_size = ar0234_enum_frame_size,
	.get_selection = ar0234_get_selection,
};

static const struct v4l2_subdev_core_ops ar0234_core_ops = {
	.subscribe_event = v4l2_ctrl_subdev_subscribe_event,
	.unsubscribe_event = v4l2_event_subdev_unsubscribe,
};

static const struct v4l2_subdev_ops ar0234_subdev_ops = {
	.core = &ar0234_core_ops,
	.video = &ar0234_video_ops,
	.pad = &ar0234_pad_ops,
};

static const struct v4l2_subdev_internal_ops ar0234_internal_ops = {
	.open = ar0234_open,
};

/* Verify chip ID */
static int ar0234_identify_module(struct ar0234 *ar0234)
{
	struct i2c_client *client = v4l2_get_subdevdata(&ar0234->sd);
	u16 val;
	int ret;

	ret = ar0234_read_reg(ar0234, AR0234_REG_CHIP_ID, &val);

	if (ret) {
		dev_err(&client->dev, "failed to read chip id %x\n",
			AR0234_CHIP_ID);
		return ret;
	}

	if (val != AR0234_CHIP_ID) {
		dev_err(&client->dev, "chip id mismatch: read %04x, expected %04x\n",
			val, AR0234_CHIP_ID);
		return -EIO;
	}

	return 0;
}

static void ar0234_free_controls(struct ar0234 *ar0234)
{
	v4l2_ctrl_handler_free(ar0234->sd.ctrl_handler);
	mutex_destroy(&ar0234->mutex);
}

static int ar0234_check_hwcfg(struct device *dev)
{
	struct fwnode_handle *endpoint;
	struct v4l2_fwnode_endpoint ep_cfg = {
		.bus_type = V4L2_MBUS_CSI2_DPHY
	};
	int ret = -EINVAL;

	endpoint = fwnode_graph_get_next_endpoint(dev_fwnode(dev), NULL);
	if (!endpoint) {
		dev_err(dev, "endpoint node not found\n");
		return -EINVAL;
	}

	if (v4l2_fwnode_endpoint_alloc_parse(endpoint, &ep_cfg)) {
		dev_err(dev, "could not parse endpoint\n");
		goto error_out;
	}

	/* Check the number of MIPI CSI2 data lanes */
	if (ep_cfg.bus.mipi_csi2.num_data_lanes != 2) {
		dev_err(dev, "only 2 data lanes are currently supported\n");
		goto error_out;
	}

	/* Check the link frequency set in device tree */
	if (!ep_cfg.nr_of_link_frequencies) {
		dev_err(dev, "link-frequency property not found in DT\n");
		goto error_out;
	}

	if (ep_cfg.nr_of_link_frequencies != 1 ||
	    ep_cfg.link_frequencies[0] != AR0234_LINK_FREQ_360MHZ) {
		dev_err(dev, "Link frequency not supported: %lld\n",
			ep_cfg.link_frequencies[0]);
		goto error_out;
	}

	ret = 0;

error_out:
	v4l2_fwnode_endpoint_free(&ep_cfg);
	fwnode_handle_put(endpoint);

	return ret;
}

static int ar0234_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ar0234 *ar0234 = to_ar0234(sd);

	v4l2_async_unregister_subdev(sd);
	media_entity_cleanup(&sd->entity);
	ar0234_free_controls(ar0234);

	pm_runtime_disable(&client->dev);
	pm_runtime_set_suspended(&client->dev);

	return 0;
}

static int ar0234_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct ar0234 *ar0234;
	int ret;

	ar0234 = devm_kzalloc(&client->dev, sizeof(*ar0234), GFP_KERNEL);
	if (!ar0234)
		return -ENOMEM;
	
	/* Get GPIOs from the device tree */
	ar0234->power_gpio = devm_gpiod_get_optional(dev, "power",
						     GPIOD_OUT_LOW);
	if (IS_ERR(ar0234->power_gpio)) {
		dev_err(dev, "Failed to get power gpio\n");
		return PTR_ERR(ar0234->power_gpio);
	}

	ar0234->reset_gpio = devm_gpiod_get_optional(dev, "reset",
						     GPIOD_OUT_LOW);
	if (IS_ERR(ar0234->reset_gpio)) {
		dev_err(dev, "Failed to get reset gpio\n");
		return PTR_ERR(ar0234->reset_gpio);
	}

	v4l2_i2c_subdev_init(&ar0234->sd, client, &ar0234_subdev_ops);

	/* Check the hardware configuration in device tree */
	if (ar0234_check_hwcfg(dev))
		return -EINVAL;
	
	ret = ar0234_identify_module(ar0234);
	if (ret) {
		dev_err(&client->dev, "failed to find sensor: %d\n", ret);
		return ret;
	}

	/* Set default mode to max resolution */
	ar0234->mode = &supported_modes[0];
	ret = ar0234_init_controls(ar0234);
	if (ret) {
		dev_err(&client->dev, "failed to init controls: %d\n", ret);
		goto error_handler_free;
	}

	/* Initialize subdev */
	ar0234->sd.internal_ops = &ar0234_internal_ops;
	ar0234->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE |
			    V4L2_SUBDEV_FL_HAS_EVENTS;
	ar0234->sd.entity.function = MEDIA_ENT_F_CAM_SENSOR;

	/* Initialize source pad */
	ar0234->pad.flags = MEDIA_PAD_FL_SOURCE;

	/* Initialize default format */
	ar0234_set_default_format(ar0234);

	ret = media_entity_pads_init(&ar0234->sd.entity, 1, &ar0234->pad);
	if (ret) {
		dev_err(dev, "failed to init entity pads: %d\n", ret);
		goto error_handler_free;
	}

	ret = v4l2_async_register_subdev_sensor_common(&ar0234->sd);
	if (ret < 0) {
		dev_err(dev, "failed to register V4L2 subdev: %d\n",ret);
		goto error_media_entity;
	}

	/* Enable runtime PM and turn off the device */
	pm_runtime_set_active(dev);
	pm_runtime_enable(dev);
	pm_runtime_idle(dev);

	return 0;

error_media_entity:
	media_entity_cleanup(&ar0234->sd.entity);

error_handler_free:
	ar0234_free_controls(ar0234);

	return ret;
}

static const struct dev_pm_ops ar0234_pm_ops = {
	.runtime_resume = ar0234_power_on,
	.runtime_suspend = ar0234_power_off,
};

static const struct of_device_id ar0234_dt_ids[] = {
	{ .compatible = "onsemi,ar0234" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, ar0234_dt_ids);

static struct i2c_driver ar0234_i2c_driver = {
	.driver = {
		.name = "ar0234",
		.of_match_table = ar0234_dt_ids,
		.pm = &ar0234_pm_ops,
	},
	.probe_new = ar0234_probe,
	.remove = ar0234_remove,
};

module_i2c_driver(ar0234_i2c_driver);

MODULE_AUTHOR("Eren HALI <ehali@deico.com.tr>");
MODULE_DESCRIPTION("ON Semiconductor ar0234 sensor driver");
MODULE_LICENSE("GPL v2");