/*
 * hd44780.c
 *
 *  Created on: Feb 5, 2017
 *      Author: ctomasin
 */

#include <common.h>
#include <led-display.h>
#include <asm/gpio.h>
#include <dm.h>
#include <dm/pinctrl.h>

#define DEBUG

#ifdef CONFIG_CMD_DISPLAY

// TODO: impostare i valori corretti
static const int pin_rs = 0;
static const int pin_en;
static const int pin_db7;
static const int pin_db6;
static const int pin_db5;
static const int pin_db4;

static void hd44780_pulse_enable(void)
{
	gpio_set_value(pin_en,0);
	udelay(1);
	gpio_set_value(pin_en,1);
	udelay(1);
	gpio_set_value(pin_en,0);
	udelay(100);
}

static void hd44780_write_4_bits(uint8_t val)
{
	gpio_set_value(pin_db7,val & (1 << 3));
	gpio_set_value(pin_db6,val & (1 << 2));
	gpio_set_value(pin_db5,val & (1 << 1));
	gpio_set_value(pin_db4,val & (1 << 0));
	hd44780_pulse_enable();

}

static void hd44780_send(uint8_t val, uint8_t mode)
{
	gpio_set_value( pin_rs,mode);
	hd44780_write_4_bits(val >> 4);
	hd44780_write_4_bits(val);
}

static void command(uint8_t val)
{
	hd44780_send(val,0);
}

static void hd44780_data(uint8_t val)
{
	hd44780_send(val,1);
}

static void hd44870_init(void)
{
	static int sInitialized = 0;
	if(!sInitialized) {

		struct udevice *pinctrl_dev;
		uclass_first_device(UCLASS_PINCTRL,&pinctrl_dev);
		if (pinctrl_dev) {
			pinctrl_select_state(pinctrl_dev,"default");
		}

		struct udevice *gpio_dev;
		uclass_first_device(UCLASS_GPIO,&gpio_dev);
		if (gpio_dev) {
			// HD44780U manual pag. 46 (fig. 24)
			gpio_direction_output(pin_rs,0);
			gpio_direction_output(pin_en,0);
			gpio_direction_output(pin_db7,0);
			gpio_direction_output(pin_db6,0);
			gpio_direction_output(pin_db5,0);
			gpio_direction_output(pin_db4,0);
		}

		mdelay(40);
		hd44780_write_4_bits(0x03);

		udelay(4500);
		hd44780_write_4_bits(0x03);

		udelay(150);
		hd44780_write_4_bits(0x03);

		hd44780_write_4_bits(0x02);
		sInitialized = 1;
	}
}

void display_set (int cmd)
{
	hd44870_init();
	if(DISPLAY_CLEAR & cmd) {
		command(0x01);
		mdelay(2);
	}
	if(DISPLAY_HOME & cmd) {
		command(0x02);
		mdelay(2);
	}
#ifdef DEBUG
	if(DISPLAY_CLEAR & cmd) {
		puts("<clearing display>\n");
	}
	if(DISPLAY_HOME & cmd) {
		puts("<home>\n");
	}
#endif
}

int display_putc (char c)
{
	hd44870_init();
	hd44780_data(c);
	int rc = c;
#ifdef DEBUG
	putc(c);
#endif
	return rc;
}
#endif // CONFIG_CMD_DISPLAY
