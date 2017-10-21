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

MODULE_AUTHOR("Carlo Tomasin <c.tomasin@gmail.com>");
MODULE_DESCRIPTION("Device driver for HD4480 2-lines LCD display");
MODULE_LICENSE("GPLv3");
