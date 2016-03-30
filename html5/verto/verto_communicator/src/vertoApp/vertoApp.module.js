(function() {
  'use strict';

  var vertoApp = angular.module('vertoApp', [
    'timer',
    'ngRoute',
    'vertoControllers',
    'vertoDirectives',
    'vertoFilters',
    'ngStorage',
    'ngAnimate',
    'ngSanitize',
    'toastr',
    'FBAngular',
    'cgPrompt',
    '720kb.tooltips',
    'ui.gravatar',
    'ui.bootstrap',
    'directive.g+signin',
    'pascalprecht.translate',
  ]);

  vertoApp.config(['$routeProvider', 'gravatarServiceProvider', '$translateProvider',
    function($routeProvider, gravatarServiceProvider, $translateProvider) {
      $routeProvider.
      when('/', {
        title: 'Loading',
        templateUrl: 'partials/splash_screen.html',
        controller: 'SplashScreenController'
      }).
      when('/login', {
        title: 'Login',
        templateUrl: 'partials/login.html',
        controller: 'LoginController'
      }).
      when('/dialpad', {
        title: 'Dialpad',
        templateUrl: 'partials/dialpad.html',
        controller: 'DialPadController'
      }).
      when('/incall', {
          title: 'In a Call',
          templateUrl: 'partials/incall.html',
          controller: 'InCallController'
        }).
      when('/preview', {
        title: 'Preview Video',
        templateUrl: 'partials/preview.html',
        controller: 'PreviewController'
      }).
      when('/browser-upgrade', {
        title: '',
        templateUrl: 'partials/browser_upgrade.html',
        controller: 'BrowserUpgradeController'
      }).
      otherwise({
        redirectTo: '/'
      });

      gravatarServiceProvider.defaults = {
        default: 'mm'  // Mystery man as default for missing avatars
      };


      $translateProvider
      .useStaticFilesLoader({
        prefix: 'locales/locale-',
        suffix: '.json'
      })
      .registerAvailableLanguageKeys(['en', 'it', 'pt', 'fr', 'de', 'es', 'pl', 'ru', 'sv', 'id'], {
        'en': 'en',
        'en_GB': 'en',
        'en_US': 'en',
        'it': 'it',
        'it_IT': 'it',
        'fr': 'fr',
        'fr_FR': 'fr',
        'fr_CA': 'fr',
        'pt': 'pt',
        'pt_BR': 'pt',
        'pt_PT': 'pt',
        'de': 'de',
        'de_DE': 'de',
        'es': 'es',
        'es_ES': 'es',
        'pl': 'pl',
        'pl_PL': 'pl',
        'ru': 'ru',
        'ru_RU': 'ru',
        'sv': 'sv',
        'sv_SV': 'sv',
        'sv_FI': 'sv',
		'id': 'id',
        'id_ID': 'id'
      })
      .preferredLanguage('en')
      .determinePreferredLanguage()
      .fallbackLanguage('en')
      .useSanitizeValueStrategy(null);
    }
  ]);

  vertoApp.run(['$rootScope', '$location', 'toastr', 'prompt', 'verto',
    function($rootScope, $location, toastr, prompt, verto) {

      $rootScope.$on( "$routeChangeStart", function(event, next, current) {
        if (!verto.data.connected) {
          if ( next.templateUrl === "partials/login.html") {
            // pass
          } else {
            $location.path("/");
          }
        }
      });

      $rootScope.$on('$routeChangeSuccess', function(event, current, previous) {
        $rootScope.title = current.$$route.title;
      });

      $rootScope.safeProtocol = false;

      if (window.location.protocol == 'https:') {
        $rootScope.safeProtocol = true;
      }


      $rootScope.promptInput = function(title, message, label, callback) {
        var ret = prompt({
          title: title,
          message: message,
          input: true,
          label: label
        }).then(function(ret) {
          if (angular.isFunction(callback)) {
            callback(ret);
          }
        }, function() {

        });

      };

    }
  ]);

})();
