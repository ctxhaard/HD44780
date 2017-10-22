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

// TODO: il modulo deve creare un character device che permette solo operazioni di:
// - scrittura : scrive direttamente nella prima posizione tutti i caratteri indicati; la riga sottostante è una
//			mera continuazione della riga sopra
// - lseek     : permette di posizionarsi direttamente su una certa posizione ed andare ad aggiornare solo questa
//			(in modo da velocizzare le operazioni in cui una parte del testo è fissa e una parte viene aggiornata
//			spesso; es: "distanza: 132 cm"

// creazione del dispositivo a caratteri
// probe
// module init

static int hd44780_probe(struct platform_device *pdev)
{
	return 0;
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
	platform_driver_register(&hd44780_platform_driver);
	pr_debug("hd4480 module init");
	return 0;
}

static void __exit hd44780_exit(void)
{
	pr_debug("hd4480 module exit");
}

module_init(hd44780_init)
module_exit(hd44780_exit)

MODULE_AUTHOR("Carlo Tomasin <c.tomasin@gmail.com>");
MODULE_DESCRIPTION("Device driver for HD4480 2-lines LCD display");
MODULE_LICENSE("GPL");
