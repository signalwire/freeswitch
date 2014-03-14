/*
 * The FreeSWITCH Portal Project
 * Copyright (C) 2013-2013, Seven Du <dujinfang@gmail.com>
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
 * The Original Code is The FreeSWITCH Portal Project Software/Application
 *
 * The Initial Developer of the Original Code is
 * Seven Du <dujinfang@gmail.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Seven Du <dujinfang@gmail.com>
 *
 *
 * fsportal.js -- The FreeSWITCH Portal Project
 *
 */

var App = Ember.Application.create({
	LOG_TRANSITIONS: true,
	rootElement: $('#container'),
	total: 0,
	ready: function(){
		$.get("/txtapi/status", function(data){
			$('#serverStatus').html("<pre>" + data + "</pre>");
		});
	}
});

App.ApplicationRoute = Ember.Route.extend({
	setupController: function(controller) {
		// alert("setupController");
	},
	actions: {
		newUser: function() {
			return Bootstrap.ModalManager.show('newUserForm');
		}
	}
});

App.CallsRoute = Ember.Route.extend({
	setupController: function(controller) {
		// Set the IndexController's `title`
		// controller.set('title', "My App");
		// alert("a")
		console.log("callsRoute");
		App.callsController.load();
  	}//,
  	// renderTemplate: function() {
	// this.render('calls');
  	// }
});

App.ChannelsRoute = Ember.Route.extend({
	setupController: function(controller) {
		// Set the IndexController's `title`
		// controller.set('title', "My App");
		// alert("a")
		console.log("callsRoute");
		App.channelsController.load();
  	}//,
  	// renderTemplate: function() {
		// this.render('calls');
  	// }
});

App.ShowRegistrationsRoute = Ember.Route.extend({
	setupController: function(controller) {
		// Set the Controller's `title`
		controller.set('title', "ShowRegistrations");
		App.registrationsController.load();
	}//,
	// renderTemplate: function() {
		// this.render('calls');
	// }
});

App.ShowModulesRoute = Ember.Route.extend({
	setupController: function(controller) {
		// Set the Controller's `title`
		App.showModulesController.load();
  	}//,
  	// renderTemplate: function() {
		// this.render('calls');
  	// }
});

App.ShowApplicationsRoute = Ember.Route.extend({
	setupController: function(controller) {
		// Set the Controller's `title`
		controller.set('title', "ShowApplications");
		console.log("showApplications");
		App.applicationsController.load();
	}//,
	// renderTemplate: function() {
		// this.render('calls');
	// }
});

App.ShowEndpointsRoute = Ember.Route.extend({
	setupController: function(controller) {
		// Set the Controller's `title`
		controller.set('title', "ShowEndpoints");
		console.log(controller);
		App.showEndpointsController.load();
  	}//,
  	// renderTemplate: function() {
		// this.render('calls');
  	// }
});

App.ShowCodecsRoute = Ember.Route.extend({
	setupController: function(controller) {
		App.showCodecsController.load();
  	}
});

App.ShowFilesRoute = Ember.Route.extend({
	setupController: function(controller) {
		App.showFilesController.load();
	}
});

App.ShowAPIsRoute = Ember.Route.extend({
	setupController: function(controller) {
		App.showAPIsController.load();
	}
});

App.ShowAliasesRoute = Ember.Route.extend({
	setupController: function(controller) {
		App.showAliasesController.load();
	}
});

App.ShowCompletesRoute = Ember.Route.extend({
	setupController: function(controller) {
		App.showCompletesController.load();
	}
});

App.ShowManagementsRoute = Ember.Route.extend({
	setupController: function(controller) {
		App.showManagementsController.load();
	}
});

App.ShowSaysRoute = Ember.Route.extend({
	setupController: function(controller) {
		App.showSaysController.load();
	}
});

App.ShowNatMapsRoute = Ember.Route.extend({
	setupController: function(controller) {
		App.showNatMapsController.load();
	}
});

App.ShowChatsRoute = Ember.Route.extend({
	setupController: function(controller) {
		App.showChatsController.load();
	}
});

App.ShowInterfacesRoute = Ember.Route.extend({
	setupController: function(controller) {
		App.showInterfacesController.load();
	}
});

App.ShowInterfaceTypesRoute = Ember.Route.extend({
	setupController: function(controller) {
		App.showInterfaceTypesController.load();
	}
});

App.ShowTasksRoute = Ember.Route.extend({
	setupController: function(controller) {
		App.showTasksController.load();
	}
});

App.ShowLimitsRoute = Ember.Route.extend({
	setupController: function(controller) {
		App.showLimitsController.load();
	}
});

App.UsersRoute = Ember.Route.extend({
	setupController: function(controller) {
		App.usersController.load();
	}
});

App.newUserRoute = Ember.Route.extend({
	setupController: function(Controller) {
		alert("auto_update_calls");
	},
	actions: {
		show: function(controller) {
			App.newUserController.show();
		}
	}
});

App.SofiaStatusRoute = Ember.Route.extend({
	setupController: function(controller) {
		App.sofiaStatusController.load();
	}
});

App.Router.map(function(){
	this.route("calls");
	this.route("channels");
	this.route("showRegistrations");
	this.route("showModules");
	this.route("showApplications");
	this.route("showEndpoints");
	this.route("showCodecs");
	this.route("showFiles");
	this.route("showAPIs");
	this.route("showAliases");
	this.route("showCompletes");
	this.route("showManagements");
	this.route("showNatMaps");
	this.route("showSays");
	this.route("showChats");
	this.route("showInterfaces");
	this.route("showInterfaceTypes");
	this.route("showTasks");
	this.route("showLimits");
	this.route("show");
	this.route("users");
	this.route("newUser");
	this.route("sofiaStatus");
	this.route("addGateway");
	this.route("about", { path: "/about" });
});

App.User = Em.Object.extend({
	id: null,
	context: null,
	domain: null,
	group: null,
	contact: null
});

App.Call = Em.Object.extend({
	uuid: null,
	cidName: null,
	cidNumber: null

});

App.Channel = Em.Object.extend({
	uuid: null,
	cidName: null,
	cidNumber: null

});

App.ApplicationController = Ember.ObjectController.extend({
	actions: {
		newUser: function() {
			alert("ApplicationController");
		}
	}
});

App.callsController = Ember.ArrayController.create({
	content: [],
	init: function(){
	},
	load: function() {
		var me = this;
		$.getJSON("/txtapi/show?calls%20as%20json", function(data){
			  // var channels = JSON.parse(data);
			console.log(data.row_count);
			me.set('total', data.row_count);
			me.content.clear();
			if (data.row_count == 0) return;

			// me.pushObjects(data.rows);
			data.rows.forEach(function(r) {
				me.pushObject(App.Call.create(r));
			});

		});
	},
	delete: function(uuid) {
		var obj = this.content.findProperty("uuid", uuid);
		if (obj) this.content.removeObject(obj);// else alert(uuid);
	},
	dump: function(uuid) {
		var obj = this.content.findProperty("uuid", uuid);
		console.log(obj.getProperties(["uuid", "cid_num"]));
	},
	raw: function() {
		$.get("/api/show?calls", function(data){
			$('#aa').html(data);
		});
	}
});

App.channelsController = Ember.ArrayController.create({
	content: [],
	listener: undefined,
	init: function(){
	},
	load: function() {
		var me = this;
		$.getJSON("/txtapi/show?channels%20as%20json", function(data){
			  // var channels = JSON.parse(data);
		 	console.log(data.row_count);
			me.set('total', data.row_count);
			me.content.clear();
			if (data.row_count == 0) return;
			data.rows.forEach(function(row) {
				me.pushObject(App.Channel.create(row));
		 	});

		});
	},
	delete: function(uuid) {
		var obj = this.content.findProperty("uuid", uuid);
		if (obj) this.content.removeObject(obj);// else alert(uuid);
	},
	dump: function(uuid) {
		var obj = this.content.findProperty("uuid", uuid);
		console.log(obj.getProperties(["uuid", "cid_num"]));
	},
	checkEvent: function () { // event_sink with json is not yet support in FS
		console.log("check");
		var me = this;
		if (!this.get("listener")) {
			$.getJSON("/api/event_sink?command=create-listener&events=ALL&format=json", function(data){
				console.log(data);
				if (data.listener) {
					me.set("listener", data.listener["listen-id"]);
				}
			});
		}
		if (!me.get("listener")) return;

		$.getJSON("/api/event_sink?command=check-listener&listen-id=" +
			me.get("listener") + "&format=json", function(data){
			console.log(data);
			if (!data.listener) {
				me.set("listener", undefined);
			} else {
				data.events.forEach(function(e) {
					eventCallback(e);
				});
			}
		});
	},
	checkXMLEvent: function() {
		console.log("check XML Event");
		var me = this;
		if (!this.get("listener")) {
			$.get("/api/event_sink?command=create-listener&events=ALL", function(data){
				// console.log(data);
				var listen_id = data.getElementsByTagName("listen-id")[0];
				if (listen_id) {
					me.set("listener", listen_id.textContent);
				}
			});
		}

		if (!me.get("listener")) return;

		$.get("/api/event_sink?command=check-listener&listen-id=" + me.get("listener"), function(data){
			// console.log(data);
			var listener = data.getElementsByTagName("listener")[0];
			if (!listener) {
				me.set("listener", undefined);
			} else {
				var events = data.getElementsByTagName("event");
				for (var i=0; i<events.length; i++) {
					var e = {};
					var headers = events[i].getElementsByTagName("headers")[0];
					for (var j=0; j<headers.childNodes.length; j++) {
						e[headers.childNodes[j].nodeName] = headers.childNodes[j].textContent;
					}
					// console.log(e);
					eventCallback(e);
				}
			}
		});
	}

});

App.registrationsController = Ember.ArrayController.create({
	content: [],
	init: function(){
	},
	load: function() {
		var me = this;
		$.getJSON("/txtapi/show?registrations%20as%20json", function(data){
			  // var channels = JSON.parse(data);
			console.log(data.row_count);
			me.set('total', data.row_count);
			me.content.clear();
			if (data.row_count == 0) return;

			me.pushObjects(data.rows);

		});
	}
});

App.applicationsController = Ember.ArrayController.create({
	content: [],
	init: function(){
	},
	load: function() {
		var me = this;
		$.getJSON("/txtapi/show?application%20as%20json", function(data){
			  // var channels = JSON.parse(data);
			console.log(data.row_count);
			me.set('total', data.row_count);
			me.content.clear();
			if (data.row_count == 0) return;

			me.pushObjects(data.rows);

		});
	}
});

App.showEndpointsController = Ember.ArrayController.create({
	content: [],
	init: function(){
	},
	load: function() {
		var me = this;
		$.getJSON("/txtapi/show?endpoints%20as%20json", function(data){
			  // var channels = JSON.parse(data);
			console.log(data.row_count);
			me.set('total', data.row_count);
			me.content.clear();
			if (data.row_count == 0) return;

			me.pushObjects(data.rows);

		});
	}
});

App.showCodecsController = Ember.ArrayController.create({
	content: [],
	init: function(){
	},
	load: function() {
		var me = this;
		$.getJSON("/txtapi/show?codec%20as%20json", function(data){
			  // var channels = JSON.parse(data);
			console.log(data.row_count);
			me.set('total', data.row_count);
			me.content.clear();
			if (data.row_count == 0) return;

			me.pushObjects(data.rows);

		});
	}
});

App.showFilesController = Ember.ArrayController.create({
	content: [],
	init: function(){
	},
	load: function() {
		var me = this;
		$.getJSON("/txtapi/show?files%20as%20json", function(data){
			  // var channels = JSON.parse(data);
			me.set('total', data.row_count);
			me.content.clear();
			if (data.row_count == 0) return;

			me.pushObjects(data.rows);

		});
	}
});

App.showAPIsController = Ember.ArrayController.create({
	content: [],
	init: function(){
	},
	load: function() {
		var me = this;
		$.getJSON("/txtapi/show?api%20as%20json", function(data){
			  // var channels = JSON.parse(data);
			me.set('total', data.row_count);
			me.content.clear();
			if (data.row_count == 0) return;

			var rows = [];
			data.rows.forEach(function(r) {
				if (r.name == "show") {
					r.syntax = r.syntax.replace(/\|/g, "\n");
				} else if (r.name == "fsctl") {
					r.syntax = r.syntax.replace(/\]\|/g, "]\n");
				} else {
					r.syntax = r.syntax.replace(/\n/g, "\n");
				}
				// console.log(r.syntax);
				rows.push(r);
			});

			me.pushObjects(rows);

		});
	}
});

App.showModulesController = Ember.ArrayController.create({
	content: [],
	init: function(){
	},
	load: function() {
		var me = this;
		$.getJSON("/txtapi/show?module%20as%20json", function(data){
			me.set('total', data.row_count);
			me.content.clear();
			console.log(data);
			if (data.row_count == 0) return;

			me.pushObjects(data.rows);

		});
	}
});

App.showAliasesController = Ember.ArrayController.create({
	content: [],
	init: function(){
	},
	load: function() {
		var me = this;
		$.getJSON("/txtapi/show?aliases%20as%20json", function(data){
			me.set('total', data.row_count);
			me.content.clear();
			if (data.row_count == 0) return;

			me.pushObjects(data.rows);

		});
	}
});

App.showCompletesController = Ember.ArrayController.create({
	content: [],
	init: function(){
	},
	load: function() {
		var me = this;
		$.getJSON("/txtapi/show?complete%20as%20json", function(data){
			me.set('total', data.row_count);
			me.content.clear();
			if (data.row_count == 0) return;

			me.pushObjects(data.rows);

		});
	}
});

App.showManagementsController = Ember.ArrayController.create({
	content: [],
	init: function(){
	},
	load: function() {
		var me = this;
		$.getJSON("/txtapi/show?management%20as%20json", function(data){
			me.set('total', data.row_count);
			me.content.clear();
			if (data.row_count == 0) return;

			me.pushObjects(data.rows);

		});
	}
});

App.showNatMapsController = Ember.ArrayController.create({
	content: [],
	init: function(){
	},
	load: function() {
		var me = this;
		$.getJSON("/txtapi/show?nat_map%20as%20json", function(data){
			me.set('total', data.row_count);
			me.content.clear();
			if (data.row_count == 0) return;

			me.pushObjects(data.rows);

		});
	}
});

App.showSaysController = Ember.ArrayController.create({
	content: [],
	init: function(){
	},
	load: function() {
		var me = this;
		$.getJSON("/txtapi/show?say%20as%20json", function(data){
			me.set('total', data.row_count);
			me.content.clear();
			if (data.row_count == 0) return;

			me.pushObjects(data.rows);

		});
	}
});

App.showChatsController = Ember.ArrayController.create({
	content: [],
	init: function(){
	},
	load: function() {
		var me = this;
		$.getJSON("/txtapi/show?chat%20as%20json", function(data){
			me.set('total', data.row_count);
			me.content.clear();
			if (data.row_count == 0) return;

			me.pushObjects(data.rows);

		});
	}
});

App.showInterfacesController = Ember.ArrayController.create({
	content: [],
	init: function(){
	},
	load: function() {
		var me = this;
		$.getJSON("/txtapi/show?interfaces%20as%20json", function(data){
			me.set('total', data.row_count);
			me.content.clear();
			if (data.row_count == 0) return;

			me.pushObjects(data.rows);

		});
	}
});

App.showInterfaceTypesController = Ember.ArrayController.create({
	content: [],
	init: function(){
	},
	load: function() {
		var me = this;
		$.getJSON("/txtapi/show?interface_types%20as%20json", function(data){
			me.set('total', data.row_count);
			me.content.clear();
			if (data.row_count == 0) return;

			me.pushObjects(data.rows);

		});
	}
});

App.showTasksController = Ember.ArrayController.create({
	content: [],
	init: function(){
	},
	load: function() {
		var me = this;
		$.getJSON("/txtapi/show?tasks%20as%20json", function(data){
			me.set('total', data.row_count);
			me.content.clear();
			if (data.row_count == 0) return;

			me.pushObjects(data.rows);

		});
	}
});

App.showLimitsController = Ember.ArrayController.create({
	content: [],
	init: function(){
	},
	load: function() {
		var me = this;
		$.getJSON("/txtapi/show?limit%20as%20json", function(data){
			me.set('total', data.row_count);
			me.content.clear();
			if (data.row_count == 0) return;

			me.pushObjects(data.rows);

		});
	}
});


App.usersController = Ember.ArrayController.create({
	content: [],
	init: function(){
	},
	load: function() {
		var me = this;
		$.get("/txtapi/list_users", function(data){
			  // var channels = JSON.parse(data);
			lines = data.split("\n");
			me.content.clear();
			var users = [];
			for (var i=1; i<lines.length; i++) {
				var line = lines[i];
				var fields = line.split("|");
				if (fields.length == 1) break;
				var user = {
					id: fields.shift(),
					context: fields.shift(),
					domain: fields.shift(),
					group: fields.shift(),
					contact: fields.shift(),
					callgroup: fields.shift(),
					cid_name: fields.shift(),
					cid_number: fields.shift()
				}
				// me.pushObject(App.User.create(user));
				users.push(App.User.create(user));
			}
				me.pushObjects(users);
		});
	}
});


App.UsersController = Ember.ObjectController.extend({
  newUserButtons: [
    Ember.Object.create({title: 'Create', clicked:"submit", type:"primary"}),
    Ember.Object.create({title: 'Cancel', clicked: "cancel", dismiss: 'modal'})
  ],

  actions: {
    //Submit the modal
    submit: function() {
	$.post("/txtapi/lua?portal/create_user.lua%20" + $("#user_id").val(), {
		data: "user_id=xxxx",
		success: function() { },
		error: function(e) { }
	});

	// Bootstrap.NM.push('Successfully submitted modal', 'success');
	return Bootstrap.ModalManager.hide('newUserForm');
    },

    //Cancel the modal, we don't need to hide the model manually because we set {..., dismiss: 'modal'} on the button meta data
    cancel: function() {
		Bootstrap.ModalManager.hide('newUserForm');
		return Bootstrap.NM.push('Modal was cancelled', 'info');
    },

    //Show the modal
    newUser: function() {
		return Bootstrap.ModalManager.show('newUserForm');
    }
  }
});

App.sofiaStatusController = Ember.ArrayController.create({
	content: [],
	init: function(){
	},
	load: function() {
		var me = this;
		$.get("/xmlapi/sofia?xmlstatus", function(data){
			console.log(data);
			var row_count = 0;
			var aliases = data.getElementsByTagName("alias");
			var profiles = data.getElementsByTagName("profile");
			var gateways = data.getElementsByTagName("gateway");

			me.content.clear();

			for (var i=0; i<aliases.length; i++) {
				var row = {};
				row.name = aliases[i].getElementsByTagName("name")[0].textContent;
				row.type = aliases[i].getElementsByTagName("type")[0].textContent;
				row.data = aliases[i].getElementsByTagName("data")[0].textContent;
				row.state = aliases[i].getElementsByTagName("state")[0].textContent;
				console.log(row)
				row_count++;
				me.pushObject(row);
			}

			for (var i=0; i<profiles.length; i++) {
				var row = {};
				row.name = profiles[i].getElementsByTagName("name")[0].textContent;
				row.type = profiles[i].getElementsByTagName("type")[0].textContent;
				row.data = profiles[i].getElementsByTagName("data")[0].textContent;
				row.state = profiles[i].getElementsByTagName("state")[0].textContent;
				row.actions = "Start | Stop | Restart | More ...";
				console.log(row)
				row_count++;
				me.pushObject(row);
			}

			for (var i=0; i<gateways.length; i++) {
				var row = {};
				row.name = gateways[i].getElementsByTagName("name")[0].textContent;
				row.type = gateways[i].getElementsByTagName("type")[0].textContent;
				row.data = gateways[i].getElementsByTagName("data")[0].textContent;
				row.state = gateways[i].getElementsByTagName("state")[0].textContent;
				row.actions = "Reg | UnReg | Delete";
				console.log(row)
				row_count++;
				me.pushObject(row);
			}

			me.set('total', row_count);
		});

	}
});

App.SofiaStatusController = Ember.ObjectController.extend({
	addGatewayButtons: [
		Ember.Object.create({title: 'Add', clicked:"submit", type:"primary"}),
		Ember.Object.create({title: 'Cancel', clicked: "cancel", dismiss: 'modal'})
	],

	actions: {
		//Submit the modal
		submit: function() {
			// alert("Not implemented");
			// return false;
			url = "/txtapi/lua?portal/create_gateway.lua%20" +
				$("#gateway_name").val() + "%20" +
				$("#gateway_realm").val() + "%20" +
				$("#gateway_username").val() + "%20" +
				$("#gateway_password").val() + "%20" +
				$("#gateway_register").is(":checked");
			$.post(url, {
				success: function() { },
				error: function(e) { }
			});

			// Bootstrap.NM.push('Successfully submitted modal', 'success');
			return Bootstrap.ModalManager.hide('newUserForm');
		},

		//Cancel the modal, we don't need to hide the model manually because we set {..., dismiss: 'modal'} on the button meta data
		cancel: function() {
			Bootstrap.ModalManager.hide('newUserForm');
			return Bootstrap.NM.push('Modal was cancelled', 'info');
		},

		//Show the modal
		addGateway: function() {
			return Bootstrap.ModalManager.show('newUserForm');
		}
	}
});

// App.initialize();
var global_debug_event = false;
var global_background_job = false;

function eventCallback(data) {
	console.log(data["Event-Name"]);

	if (global_debug_event ||
		(global_background_job && data["Event-Name"] == "BACKGROUND_JOB")) {
		console.log(data);
	}

	if (data["Event-Name"] == "CHANNEL_CREATE") {
		var channel = {
			uuid: data["Unique-ID"],
			cid_num: data["Caller-Caller-ID-Number"],
			dest: data["Caller-Destination-Number"],
			callstate: data["Channel-Call-State"],
			direction: data["Call-Direction"]
		}
		App.channelsController.pushObject(App.Channel.create(channel));

		var x = $('#auto_update_calls')[0];
		if (typeof x != "undefined" && x.checked) {
			return;
		}

		App.callsController.pushObject(App.Call.create(channel));
	} else if (data["Event-Name"] == "CHANNEL_HANGUP_COMPLETE") {
		App.channelsController.delete(data["Unique-ID"]);

		var x = $('#auto_update_calls')[0];
		if (typeof x != "undefined" && x.checked) {
			return;
		}

		App.callsController.delete(data["Unique-ID"]);
	} else if (data["Event-Name"] == "CHANNEL_BRIDGE") {
		var x = $('#auto_update_calls')[0];
		if (typeof x != "undefined" && x.checked) {
			return;
		}

		App.callsController.delete(data["Unique-ID"]);
		App.callsController.delete(data["Other-Leg-Unique-ID"]);

		var call = {
			uuid: data["Unique-ID"],
			b_uuid: data["Other-Leg-Unique-ID"],
			cid_num: data["Caller-Caller-ID-Number"],
			b_cid_num: data["Other-Leg-Caller-ID-Number"],
			dest: data["Caller-Destination-Number"],
			b_dest: data["Other-Leg-Destination-Number"],
			callstate: data["Channel-Call-State"],
			b_callstate: data["Channel-Call-State"],
			direction: data["Call-Direction"],
			b_direction: data["Other-Leg-Direction"],
			created: data["Caller-Channel-Created-Time"]
		};

		App.callsController.pushObject(App.Call.create(call));

	} else if (data["Event-Name"] == "CHANNEL_CALLSTATE") {
		var obj = App.channelsController.content.findProperty("uuid", data["Unique-ID"]);
		if (obj) {
			obj.set("callstate", data["Channel-Call-State"]);
		}

	}
}

// execute api
function api(cmdstr)
{
	cmdarr = cmdstr.split(" ");
	cmd = cmdarr.shift();
	arg = escape(cmdarr.join(" "));
	arg = arg ? "?" + arg : "";
	url = "/txtapi/" + cmd + arg;
	$.get(url, function(data){
		console.log(data);
	});
	return url;
}

//execute bgapi
function bgapi(cmd)
{
	if (!global_background_job) {
		socket.send("event json BACKGROUND_JOB");
		global_background_job = true;
	}
	api("bgapi " + cmd);
}

// subscribe event
function event(e)
{
	cmd = "event json " + e;
	socket.send(cmd);
	return cmd;
}
