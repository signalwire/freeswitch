(function() {
	'use strict';

	angular
	.module('vertoControllers')
	.controller('LoginController', ['$scope', '$http', '$location', 'verto',
		function($scope, $http, $location, verto) {
				$scope.checkBrowser();

			/*
			 * Load the Configs before logging in
			 * with cache buster
			 */

			$http.get(window.location.pathname + '/config.json?cachebuster=' + Math.floor((Math.random()*1000000)+1))
				.success(function(data) {

				/* save these for later as we're about to possibly over write them */
				var name = verto.data.name;
				var email = verto.data.email;

				angular.extend(verto.data, data);

				/**
				 * use stored data (localStorage) for login, allow config.json to take precedence
				 */

				if (name != '' && data.name == '') {
					verto.data.name = name;
				}
				if (email != '' && data.email == '') {
					verto.data.email = email;
				}
				if (verto.data.login == '' && verto.data.password == '' && $scope.storage.data.login != '' && $scope.storage.data.password != '') {
					verto.data.login = $scope.storage.data.login;
					verto.data.password = $scope.storage.data.password;
				}

				if (verto.data.autologin == "true" && !verto.data.autologin_done) {
					console.debug("auto login per config.json");
					verto.data.autologin_done = true;
					$scope.login();
				}
			});

			verto.data.name = $scope.storage.data.name;
			verto.data.email = $scope.storage.data.email;

			console.debug('Executing LoginController.');
		}
	]);

})();

