/*
 * hd44780.c - Controller driver for HD44780 2-lines LCD display
 * connected to GPIO lines.
 *
 * Copyright (C) 2017 Carlo Tomasin
 *
 * Author: Carlo Tomasin <c.tomasin@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#define DEBUG

#include <linux/module.h>
#include <linux/gpio/consumer.h>
#include <linux/pinctrl/consumer.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/delay.h>

struct hd44780_data {

	struct platform_device *pdev;
	struct gpio_descs *gpios;

	struct gpio_desc *gpio_rs;
	struct gpio_desc *gpio_en;
	struct gpio_desc *gpio_db4;
	struct gpio_desc *gpio_db5;
	struct gpio_desc *gpio_db6;
	struct gpio_desc *gpio_db7;
};

// TODO: il modulo deve creare un character device che permette solo operazioni di:
// - scrittura : scrive direttamente nella prima posizione tutti i caratteri indicati; la riga sottostante è una
//			mera continuazione della riga sopra
// - lseek     : permette di posizionarsi direttamente su una certa posizione ed andare ad aggiornare solo questa
//			(in modo da velocizzare le operazioni in cui una parte del testo è fissa e una parte viene aggiornata
//			spesso; es: "distanza: 132 cm"

// creazione del dispositivo a caratteri

static void hd44780_pulse_enable(struct hd44780_data *pdata)
{
	gpiod_set_value_cansleep(pdata->gpio_en, 0);
	udelay(1);
	gpiod_set_value_cansleep(pdata->gpio_en, 1);
	udelay(1);
	gpiod_set_value_cansleep(pdata->gpio_en, 0);
	udelay(100);
}

static void hd44780_write_4_bits(struct hd44780_data *pdata, u8 val)
{
	gpiod_set_value_cansleep(pdata->gpio_db7, val & (1 << 3));
	gpiod_set_value_cansleep(pdata->gpio_db6, val & (1 << 2));
	gpiod_set_value_cansleep(pdata->gpio_db5, val & (1 << 1));
	gpiod_set_value_cansleep(pdata->gpio_db4, val & (1 << 0));
	hd44780_pulse_enable(pdata);
}

static void hd44780_send(struct hd44780_data *pdata, u8 val, u8 mode)
{
	gpiod_set_value_cansleep(pdata->gpio_rs, mode);
	hd44780_write_4_bits(pdata, val >> 4);
	hd44780_write_4_bits(pdata, val);
}

static void  hd44780_command(struct hd44780_data *pdata, u8 val)
{
	hd44780_send(pdata, val, 0);
}

static void hd44780_data(struct hd44780_data *pdata, u8 val)
{
	hd44780_send(pdata, val, 1);
}

static int hd44780_print(struct hd44780_data *pdata, char *msg) {
	// TODO: riportare ad inizio riga
	while (*msg) {
		hd44780_data(pdata, *msg);
		++msg;
	}
	return 0;
}


static int hd44780_prompt(struct hd44780_data *pdata)
{
	char *msg = "Hello!!!";

	dev_info(&pdata->pdev->dev, "showing prompt: %s\n", msg);

	hd44780_print(pdata, msg);
	return 0;
}

static int hd44780_setup(struct hd44780_data *pdata)
{
	dev_info(&pdata->pdev->dev, "setting up LCD\n");
	mdelay(40);
	hd44780_write_4_bits(pdata, 0x03);

	mdelay(5);
	hd44780_write_4_bits(pdata, 0x03);

	udelay(150);
	hd44780_write_4_bits(pdata, 0x03);

	hd44780_write_4_bits(pdata, 0x02);
	return 0;
}

static int hd44780_probe(struct platform_device *pdev)
{
	struct hd44780_data *pdata;
	struct pinctrl      *pinctrl;
	struct pinctrl_state *pstate;
	struct gpio_desc *pdesc;
	int i;

	pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	pdata->pdev = pdev;

	pinctrl = devm_pinctrl_get(&pdata->pdev->dev);
	if (IS_ERR(pinctrl)) {
		dev_err(&pdev->dev, "can't get pinctrl handle\n");
		return -EIO;
	}

	pstate = pinctrl_lookup_state(pinctrl, PINCTRL_STATE_DEFAULT);
	if (!pstate) {
		dev_err(&pdev->dev, "can't find default pinctrl state\n");
		return -EIO;
	}

	if (pinctrl_select_state(pinctrl, pstate)) {

		dev_err(&pdev->dev, "cant't select default pinctrl state\n");
		return -EIO;
	}

	pdata->gpios = devm_gpiod_get_array(&pdev->dev, "lcd", 0);
	if (IS_ERR(pdata->gpios) || pdata->gpios->ndescs < 6) {
		dev_err(&pdev->dev, "can't get gpios\n");
		return -EINVAL;
	}

	for (i = 0; i < pdata->gpios->ndescs; ++i) {
		pdesc = pdata->gpios->desc[i];
		gpiod_direction_output(pdesc, 0);
	}

	pdesc = pdata->gpios->desc[0];
	if (!pdesc) goto gpio_err;
	pdata->gpio_rs = pdesc;

	pdesc = pdata->gpios->desc[1];
	if (!pdesc) goto gpio_err;
	pdata->gpio_en = pdesc;

	pdesc = pdata->gpios->desc[2];
	if (!pdesc) goto gpio_err;
	pdata->gpio_db4 = pdesc;

	pdesc = pdata->gpios->desc[3];
	if (!pdesc) goto gpio_err;
	pdata->gpio_db5 = pdesc;

	pdesc = pdata->gpios->desc[4];
	if (!pdesc) goto gpio_err;
	pdata->gpio_db6 = pdesc;

	pdesc = pdata->gpios->desc[5];
	if (!pdesc) goto gpio_err;
	pdata->gpio_db7 = pdesc;

	hd44780_setup(pdata);

	hd44780_prompt(pdata);
	return 0;

gpio_err:
	dev_err(&pdev->dev, "can't get gpios\n");
	return -EINVAL;
}

static int hd44780_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id hd44780_match[] = {
		{
				.compatible = "hd44780",
		},
		{},
};

static struct platform_driver hd44780_platform_driver = {
		.probe = hd44780_probe,
		.remove = hd44780_remove,
		.driver = {
				.name = "HD44780",
				.owner = THIS_MODULE,
				.of_match_table = of_match_ptr(hd44780_match)
		}
};

static int __init hd44780_init(void)
{
	pr_info("hd4480 module init\n");
	return platform_driver_register(&hd44780_platform_driver);
}

static void __exit hd44780_exit(void)
{
	pr_info("hd4480 module exit\n");
	platform_driver_unregister(&hd44780_platform_driver);
}

module_init(hd44780_init)
module_exit(hd44780_exit)

MODULE_AUTHOR("Carlo Tomasin <c.tomasin@gmail.com>");
MODULE_DESCRIPTION("Device driver for HD4480 2-lines LCD display");
MODULE_LICENSE("GPL");
