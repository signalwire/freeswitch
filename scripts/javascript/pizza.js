/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2014, Anthony Minessale II <anthm@freeswitch.org>
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
 * Anthony Minessale II <anthm@freeswitch.org>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Anthony Minessale II <anthm@freeswitch.org>
 *
 *
 * pizza.js ASR Demonstration Application
 *
 */
include("js_modules/SpeechTools.jm");

function on_dtmf(a, b, c) {}

var dft_min = 100;
var dft_confirm = 600;

/***************** Initialize The Speech Detector  *****************/
var asr = new SpeechDetect(session, "lumenvox", "127.0.0.1");

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
pizza.orderObtainer.setGrammar("order", "pizza/order.gram", "result", dft_min, dft_confirm, true);
pizza.orderObtainer.setTopSound("GP-DeliveryorTakeout");
pizza.orderObtainer.setBadSound("GP-NoDeliveryorTake-out");
pizza.orderObtainer.addItem("Delivery,Pickup");

/***************** What Size? *****************/
pizza.sizeObtainer = new SpeechObtainer(asr, 1, 5000);
pizza.sizeObtainer.setGrammar("size", "pizza/size.gram", "result", dft_min, dft_confirm, true);
pizza.sizeObtainer.setTopSound("GP-Size");
pizza.sizeObtainer.setBadSound("GP-NI");
pizza.sizeObtainer.addItem("ExtraLarge,Large,Medium,Small,TotallyHumongous");

/***************** What Type Of Crust? *****************/
pizza.crustObtainer = new SpeechObtainer(asr, 1, 5000);
pizza.crustObtainer.setGrammar("crust", "pizza/crust.gram", "result", dft_min, dft_confirm, true);
pizza.crustObtainer.setTopSound("GP-Crust");
pizza.crustObtainer.setBadSound("GP-NI");
pizza.crustObtainer.addItem("HandTossed,Pan,Thin");

/***************** Specialty Or Custom? *****************/
pizza.specialtyObtainer = new SpeechObtainer(asr, 1, 5000);
pizza.specialtyObtainer.setGrammar("specialty", "pizza/specialty.gram", "result", dft_min, dft_confirm, true);
pizza.specialtyObtainer.setTopSound("GP-SpecialtyList");
pizza.specialtyObtainer.setBadSound("GP-NI");
pizza.specialtyObtainer.addItem("Hawaiian,MeatLovers,Pickle,Dali,Vegetarian");

/***************** Which Specialty? *****************/
pizza.typeObtainer = new SpeechObtainer(asr, 1, 5000);
pizza.typeObtainer.setGrammar("type", "pizza/specialtyorcustom.gram", "result", dft_min, dft_confirm, true);
pizza.typeObtainer.setTopSound("GP-SpecialtyorCustom");
pizza.typeObtainer.setBadSound("GP-NI");
pizza.typeObtainer.addItem("Specialty,Custom");

/***************** What Toppings? *****************/
pizza.toppingsObtainer = new SpeechObtainer(asr, 1, 5000);
pizza.toppingsObtainer.setGrammar("toppings", "pizza/pizza.gram", "result.toppinglist.ingredient", dft_min, 400, true);
pizza.toppingsObtainer.setTopSound("GP-Toppings");
pizza.toppingsObtainer.setBadSound("GP-NI");
pizza.toppingsObtainer.addItem("anchovie,artichoke,canadianbacon,everything,extracheese,garlic,goatcheese,bellpepper");
pizza.toppingsObtainer.addItem("mango,mushroom,olives,onions,pepperoni,pickle,pineapple,salami,sausage,shrimp,spinach,ham");

/***************** Change Delivery Or Size Or Crust, Add/Rem Toppings Or Start Over  *****************/
pizza.arsoObtainer = new SpeechObtainer(asr, 1, 5000);
pizza.arsoObtainer.setGrammar("arso", "pizza/arso.gram", "result", 500, 250, true);
pizza.arsoObtainer.setTopSound("GP-ARSO");
pizza.arsoObtainer.setBadSound("GP-NI");
pizza.arsoObtainer.addItem("delivery,size,crust,startover,add_topping,rem_topping");

/***************** Yes? No? Maybe So?  *****************/
pizza.yesnoObtainer = new SpeechObtainer(asr, 1, 5000);
pizza.yesnoObtainer.setGrammar("yesno", "pizza/yesno.gram", "result", 500, 250, true);
pizza.yesnoObtainer.setBadSound("GP-NI");
pizza.yesnoObtainer.addItem("yes,no");

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
