/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005/2006, Anthony Minessale II <anthmct@yahoo.com>
 *
 * Version: MPL 1.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is
 * Anthony Minessale II <anthmct@yahoo.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Anthony Minessale II <anthmct@yahoo.com>
 *
 *
 * pizza.js ASR Demonstration Application
 *
 */
include("js_modules/SpeechTools.jm");

function on_dtmf(a, b, c) {}

var dft_min = 40;
var dft_confirm = 70;

/***************** Initialize The Speech Detector  *****************/
var asr = new SpeechDetect(session, "pocketsphinx");

/***************** Be more verbose *****************/
asr.debug = 1;

/***************** Set audio params *****************/
asr.setAudioBase("/root/pizza/");
asr.setAudioExt(".wav");

/***************** Unload the last grammar whenever we activate a new one *****************/
asr.AutoUnload = true;

/***************** Create And Configure The Pizza *****************/
var pizza = new Object();

/***************** Delivery Or Take-Out? *****************/
pizza.orderObtainer = new SpeechObtainer(asr, 1, 5000);
pizza.orderObtainer.setGrammar("pizza_order", "", "result.interpretation.input", dft_min, dft_confirm, true);
pizza.orderObtainer.setTopSound("GP-DeliveryorTakeout");
pizza.orderObtainer.setBadSound("GP-NoDeliveryorTake-out");
pizza.orderObtainer.addItemAlias("Delivery", "Delivery");
pizza.orderObtainer.addItemAlias("Takeout,Pickup", "Pickup");

/***************** What Size? *****************/
pizza.sizeObtainer = new SpeechObtainer(asr, 1, 5000);
pizza.sizeObtainer.setGrammar("pizza_size", "", "result.interpretation.input", dft_min, dft_confirm, true);
pizza.sizeObtainer.setTopSound("GP-Size");
pizza.sizeObtainer.setBadSound("GP-NI");
pizza.sizeObtainer.addItemAlias("^Extra\\s*Large", "ExtraLarge");
pizza.sizeObtainer.addItemAlias("^Large$", "Large");
pizza.sizeObtainer.addItemAlias("^Medium$", "Medium");
pizza.sizeObtainer.addItemAlias("^Small$", "Small");
pizza.sizeObtainer.addItemAlias("^Humongous$,^Huge$,^Totally\\s*Humongous$,^Totally", "TotallyHumongous");

/***************** What Type Of Crust? *****************/
pizza.crustObtainer = new SpeechObtainer(asr, 1, 5000);
pizza.crustObtainer.setGrammar("pizza_crust", "", "result.interpretation.input", dft_min, dft_confirm, true);
pizza.crustObtainer.setTopSound("GP-Crust");
pizza.crustObtainer.setBadSound("GP-NI");
pizza.crustObtainer.addItemAlias("^Hand\\s*Tossed$,^Tossed$", "HandTossed");
pizza.crustObtainer.addItemAlias("^Chicago\\s*style$,^Chicago$", "Pan");
pizza.crustObtainer.addItemAlias("^Deep,^Pan,^Baked", "Pan");
pizza.crustObtainer.addItemAlias("^New\\s*York,^Thin", "Thin");

/***************** Specialty Or Custom? *****************/
pizza.typeObtainer = new SpeechObtainer(asr, 1, 5000);
pizza.typeObtainer.setGrammar("pizza_type", "", "result.interpretation.input", dft_min, dft_confirm, true);
pizza.typeObtainer.setTopSound("GP-SpecialtyorCustom");
pizza.typeObtainer.setBadSound("GP-NI");
pizza.typeObtainer.addItemAlias("^Specialty$,^Specialty\\s*pizza$", "Specialty");
pizza.typeObtainer.addItemAlias("^pick", "Custom");


/***************** Which Specialty? *****************/
pizza.specialtyObtainer = new SpeechObtainer(asr, 1, 5000);
pizza.specialtyObtainer.setGrammar("pizza_specialty", "", "result.interpretation.input", dft_min, dft_confirm, true);
pizza.specialtyObtainer.setTopSound("GP-SpecialtyList");
pizza.specialtyObtainer.setBadSound("GP-NI");
pizza.specialtyObtainer.addItemAlias("^Hawaii,^Hawaiian", "Hawaiian");
pizza.specialtyObtainer.addItemAlias("^Meat", "MeatLovers");
pizza.specialtyObtainer.addItemAlias("Pickle,^World", "Pickle");
pizza.specialtyObtainer.addItemAlias("^Salvador,^Dolly,^Dali", "Dali");
pizza.specialtyObtainer.addItemAlias("^Veg", "Vegetarian");


/***************** What Toppings? *****************/
pizza.toppingsObtainer = new SpeechObtainer(asr, 1, 5000);
pizza.toppingsObtainer.setGrammar("pizza_toppings", "", "result.interpretation.input", dft_min, dft_confirm, true);
pizza.toppingsObtainer.setTopSound("GP-Toppings");
pizza.toppingsObtainer.setBadSound("GP-NI");
pizza.toppingsObtainer.addItemAlias("anchovie,anchovies", "anchovies");
pizza.toppingsObtainer.addItemAlias("artichoke,artichockes", "artichoke");
pizza.toppingsObtainer.addItemAlias("canadian\\s*bacon", "canadianbacon");
pizza.toppingsObtainer.addItemAlias("everything", "everything");
pizza.toppingsObtainer.addItemAlias("extra\\s*cheese", "extracheese");
pizza.toppingsObtainer.addItemAlias("garlic", "garlic");
pizza.toppingsObtainer.addItemAlias("goat\\s*cheese", "goatcheese");
pizza.toppingsObtainer.addItemAlias("bell\\s*pepper,bell\\s*peppers", "bellpepper");
pizza.toppingsObtainer.addItemAlias("mango", "mango");
pizza.toppingsObtainer.addItemAlias("mushroom,mushrooms", "mushroom");
pizza.toppingsObtainer.addItemAlias("olives", "olives");
pizza.toppingsObtainer.addItemAlias("onion,onions", "onions");
pizza.toppingsObtainer.addItemAlias("pepperoni", "pepperoni");
pizza.toppingsObtainer.addItemAlias("pickle,pickles", "pickle");
pizza.toppingsObtainer.addItemAlias("pineapple", "pineapple");
pizza.toppingsObtainer.addItemAlias("salami", "salami");
pizza.toppingsObtainer.addItemAlias("sausage", "sausage");
pizza.toppingsObtainer.addItemAlias("shrimp", "shrimp");
pizza.toppingsObtainer.addItemAlias("spinich", "spinich");
pizza.toppingsObtainer.addItemAlias("ham", "ham");

/***************** Change Delivery Or Size Or Crust, Add/Rem Toppings Or Start Over  *****************/
pizza.arsoObtainer = new SpeechObtainer(asr, 1, 5000);
pizza.arsoObtainer.setGrammar("pizza_arso", "", "result.interpretation.input", dft_min, 50, true);
pizza.arsoObtainer.setTopSound("GP-ARSO");
pizza.arsoObtainer.setBadSound("GP-NI");
pizza.arsoObtainer.addItemAlias("^delivery$", "delivery");
pizza.arsoObtainer.addItemAlias("^size$", "size");
pizza.arsoObtainer.addItemAlias("^crust$", "crust");
pizza.arsoObtainer.addItemAlias("^start\\s*over$", "startover");
pizza.arsoObtainer.addItemAlias("^add\\s*", "add_topping");
pizza.arsoObtainer.addItemAlias("^remove\\s*", "rem_topping");

/***************** Yes? No? Maybe So?  *****************/
pizza.yesnoObtainer = new SpeechObtainer(asr, 1, 5000);
pizza.yesnoObtainer.setGrammar("pizza_yesno", "", "result.interpretation.input", dft_min, 20, true);
pizza.yesnoObtainer.setBadSound("GP-NI");
pizza.yesnoObtainer.addItemAlias("^yes,^correct", "yes");
pizza.yesnoObtainer.addItemAlias("^no", "no");

/***************** Get Some Information *****************/
pizza.get = function(params, confirm) {
	for(;;) {
		if (!session.ready()) {
			return false;
		}
		var main_items = params.run();
		if (confirm && params.needConfirm) {
			pizza.yesnoObtainer.setTopSound("Confirm" + main_items[0]);
			var items = pizza.yesnoObtainer.run();
			if (items[0] == "yes") {
				break;
			}
		} else {
			break;
		}
	}

	return main_items;
};

/***************** Is This Right? *****************/
pizza.check = function () {
	if (!session.ready()) {
		return false;
	}
	asr.streamFile("GP-You_ordered_a");
	asr.streamFile(pizza.size);
	asr.streamFile(pizza.crust);
	if (pizza.type == "Specialty") {
		asr.streamFile(pizza.specialty);
		asr.streamFile("pizza");
	} else {
		asr.streamFile("pizza");
		asr.streamFile("GP-With");
		for (key in pizza.toppings) {
			if (pizza.toppings[key] == "add") {
				asr.streamFile(key);
			}
		}

	}

	pizza.yesnoObtainer.setTopSound("GP-WasThisRight");
	items = pizza.yesnoObtainer.run();
	return items[0] == "yes" ? true : false;
};

/***************** Let's Remove The Toppings *****************/
pizza.clearToppings = function() {
	if (!session.ready()) {
		return false;
	}
	if (pizza.toppings) {
		delete pizza.toppings;
	}
	pizza.have_toppings = false;
	pizza.toppings = new Array();
}

/***************** Clean Slate *****************/
pizza.init = function() {
	if (!session.ready()) {
		return false;
	}
	pizza.add_rem = "add";
	pizza.order = pizza.size = pizza.crust = pizza.type = false;
	pizza.toppingsObtainer.setTopSound("GP-Toppings");
	pizza.specialty = false;
	pizza.clearToppings();
	pizza.said_greet = false;
}

/***************** Welcome! *****************/
pizza.greet = function () {
	if (!session.ready()) {
		return false;
	}
	if (!pizza.said_greet) {
		asr.streamFile("GP-Greeting");
		pizza.said_greet = true;
	}
};

/***************** Collect Order Type *****************/
pizza.getOrder = function() {
	if (!session.ready()) {
		return false;
	}
	if (!pizza.order) {
		var items = pizza.get(pizza.orderObtainer, true);
		pizza.order = items[0];
	}
};

/***************** Collect Size *****************/
pizza.getSize = function() {
	if (!session.ready()) {
		return false;
	}
	if (!pizza.size) {
		var items = pizza.get(pizza.sizeObtainer, true);
		pizza.size = items[0]; 
	}
};

/***************** Collect Crust *****************/
pizza.getCrust = function() {
	if (!session.ready()) {
		return false;
	}
	if (!pizza.crust) {
		var items = pizza.get(pizza.crustObtainer, true);
		pizza.crust = items[0];
	}
};

/***************** Collect Pizza Type *****************/
pizza.getType = function() {
	if (!session.ready()) {
		return false;
	}
	if (!pizza.type) {
		var items = pizza.get(pizza.typeObtainer, true);
		pizza.type = items[0]; 
	}
};
	
/***************** Collect Toppings *****************/
pizza.getToppings = function() {
	if (!session.ready()) {
		return false;
	}
	if (pizza.type == "Specialty" && !pizza.specialty) {
		var items = pizza.get(pizza.specialtyObtainer, true);
		pizza.specialty = items[0];
		pizza.have_toppings = true;
	} else if (!pizza.have_toppings) {
		toppings = pizza.get(pizza.toppingsObtainer, false);
		for(x = 0; x < toppings.length; x++) {
			pizza.toppings[toppings[x]] = pizza.add_rem;
		}
		pizza.have_toppings = true;
	}
};

/***************** Modify Pizza If You Don't Like It *****************/
pizza.fix = function() {
	if (!session.ready()) {
		return false;
	}
	asr.streamFile("GP-Wanted-No");
	arso = pizza.get(pizza.arsoObtainer, false);
	for (x = 0; x < arso.length; x++) {
		if (arso[x] == "delivery") {
			pizza.order = false;
		} else if (arso[x] == "size") {
			pizza.size = false;
		} else if (arso[x] == "crust") {
			pizza.crust = false;
		} else if (arso[x] == "startover") {
			pizza.init();
		} else {
			if (pizza.type == "Specialty") {
				asr.streamFile("GP-ChangeSpec");
				pizza.type = false;
				pizza.clearToppings();
			} else {
				pizza.have_toppings = false;
				if (arso[x] == "add_topping") {
					pizza.add_rem = "add";
					pizza.toppingsObtainer.setTopSound("GP-Adding");
				} else {
					pizza.add_rem = "rem";
					pizza.toppingsObtainer.setTopSound("GP-Remove");
				}
			}
		}
	}
};

/***************** Tie It All Together *****************/
pizza.run = function() {
	pizza.init();
	
	for(;;) {
		if (!session.ready()) {
			break;
		}
		pizza.greet();
		pizza.getOrder();
		pizza.getSize();
		pizza.getCrust();
		pizza.getType();
		pizza.getToppings();

		if (pizza.check()) {
			asr.streamFile(pizza.order);
			break;
		} else {
			pizza.fix();
		}
	}
};

/***************** Begin Program *****************/
session.answer();
pizza.run();
asr.stop();
