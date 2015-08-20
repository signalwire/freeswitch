'use strict';

/* Controllers */

var vertoControllers = angular.module('vertoControllers', ['ui.bootstrap',
  'vertoService', 'storageService'
]);


vertoControllers.filter('gravatar',
  function() {
    return function(email, size) {
      if (angular.isUndefined(size)) {
        size = 40;
      }
      var hash = md5(email);
      return 'https://secure.gravatar.com/avatar/' + hash + '?s=' + size + '&d=mm';
    }
  });


vertoControllers.controller('MainController', ['$scope', '$rootScope',
  '$location', '$modal', '$timeout', 'verto', 'storage', 'toastr',
  'Fullscreen', 'prompt',
  function($scope, $rootScope, $location, $modal,
    $timeout, verto, storage, toastr, Fullscreen, prompt) {
    console.debug('Executing MainController.');

    var myVideo = document.getElementById("webcam");
    $scope.verto = verto;
    $scope.storage = storage;
    $scope.call_history = angular.element("#call_history").hasClass('active');
    $scope.chatStatus = angular.element('#wrapper').hasClass('toggled');

    /**
     * (explanation) scope in another controller extends rootScope (singleton)
     */
    $rootScope.chat_counter = 0;
    $rootScope.activePane = 'members';
    /**
     * The number that will be called.
     * @type {string}
     */
    $rootScope.dialpadNumber = '';


    /**
     * if user data saved, use stored data for logon
     */
    if (storage.data.ui_connected && storage.data.ws_connected) {
      $scope.verto.data.name = storage.data.name;
      $scope.verto.data.email = storage.data.email;
      $scope.verto.data.login = storage.data.login;
      $scope.verto.data.password = storage.data.password;

      verto.connect(function(v, connected) {
        $scope.$apply(function() {
          if (connected) {
            toastr.success('Nice to see you again.', 'Welcome back');
            $location.path('/dialpad');
          }
        });
      });

    }

    // If verto is not connected, redirects to login page.
    if (!verto.data.connected) {
      console.debug('MainController: WebSocket not connected. Redirecting to login.');
      $location.path('/login');
    }

    /**
     * Login the user to verto server and
     * redirects him to dialpad page.
     */
    $scope.login = function() {
      var connectCallback = function(v, connected) {
        $scope.$apply(function() {
          if (connected) {
            storage.data.ui_connected = verto.data.connected;
            storage.data.ws_connected = verto.data.connected;
            storage.data.name = verto.data.name;
            storage.data.email = verto.data.email;
            storage.data.login = verto.data.login;
            storage.data.password = verto.data.password;

            console.debug('Redirecting to dialpad page.');
            toastr.success('Login successful.', 'Welcome');
            $location.path('/dialpad');
          } else {
            toastr.error('There was an error while trying to login. \
            Please try again.', 'Error');
          }
        });
      };

      verto.connect(connectCallback);
    };

    /**
     * Logout the user from verto server and
     * redirects him to login page.
     */
    $scope.logout = function() {
      var disconnect = function() {
        var disconnectCallback = function(v, connected) {
          console.debug('Redirecting to login page.');
          storage.reset();
          $location.path('/login');
        };

        if (verto.data.call) {
          verto.hangup();
        }

        $scope.closeChat();
        verto.disconnect(disconnectCallback);

        verto.hangup();
      }

      if (verto.data.call) {
        prompt({
          title: 'Oops, Active Call in Course.',
          message: 'It seems that you are in a call. Do you want to hang up?'
        }).then(function() {
          disconnect();
        });
      } else {
        disconnect();
      }

    };

    /**
     * Shows a modal with the settings.
     */
    $scope.openModalSettings = function() {
      var modalInstance = $modal.open({
        animation: $scope.animationsEnabled,
        templateUrl: 'partials/modal_settings.html',
        controller: 'ModalSettingsController',
      });

      modalInstance.result.then(
        function(result) {
          console.log(result);
        },
        function() {
          console.info('Modal dismissed at: ' + new Date());
        }
      );

      modalInstance.rendered.then(
        function() {
          jQuery.material.init();
        }
      );
    };

    $scope.openModal = function(templateUrl, controller) {
      var modalInstance = $modal.open({
        animation: $scope.animationsEnabled,
        templateUrl: templateUrl,
        controller: controller,
      });

      modalInstance.result.then(
        function(result) {
          console.log(result);
        },
        function() {
          console.info('Modal dismissed at: ' + new Date());
        }
      );

      modalInstance.rendered.then(
        function() {
          jQuery.material.init();
        }
      );

    };

    $scope.showContributors = function() {
      $scope.openModal('partials/contributors.html', 'ContributorsController');
    };

    /**
     * Updates the display adding the new number touched.
     *
     * @param {String} number - New touched number.
     */
    $rootScope.dtmf = function(number) {
      $rootScope.dialpadNumber = $scope.dialpadNumber + number;
      if (verto.data.call) {
        verto.dtmf(number);
      }
    };

    /**
     * Removes the last character from the number.
     */
    $rootScope.backspace = function() {
      var number = $rootScope.dialpadNumber;
      var len = number.length;
      $rootScope.dialpadNumber = number.substring(0, len - 1);
    };


    $scope.toggleCallHistory = function() {
      if (!$scope.call_history) {
        angular.element("#call_history").addClass('active');
        angular.element("#call-history-wrapper").addClass('active');
      } else {
        angular.element("#call_history").removeClass('active');
        angular.element("#call-history-wrapper").removeClass('active');
      }
      $scope.call_history = angular.element("#call_history").hasClass('active');
    };

    $scope.clearCallHistory = function() {
      storage.data.call_history = [];
    }

    $scope.toggleChat = function() {
      if ($scope.chatStatus && $rootScope.activePane == 'chat') {
        $rootScope.chat_counter = 0;
      }
      angular.element('#wrapper').toggleClass('toggled');
      $scope.chatStatus = angular.element('#wrapper').hasClass('toggled');
    };

    $scope.openChat = function() {
      $scope.chatStatus = false;
      angular.element('#wrapper').removeClass('toggled');
    };

    $scope.closeChat = function() {
      $scope.chatStatus = true;
      angular.element('#wrapper').addClass('toggled');
    };

    $scope.goFullscreen = function() {
      if (storage.data.userStatus != 'connected') {
        return;
      }
      $rootScope.fullscreenEnabled = !Fullscreen.isEnabled();
      if (Fullscreen.isEnabled()) {
        Fullscreen.cancel();
      } else {
        Fullscreen.enable(document.getElementsByTagName('body')[0]);
      }
    }

    $rootScope.$on('call.video', function(event) {
      storage.data.videoCall = true;
    });

    $rootScope.$on('call.hangup', function(event, data) {
      if (Fullscreen.isEnabled()) {
        Fullscreen.cancel();
      }


      console.log($scope.chatStatus);
      if (!$scope.chatStatus) {
        angular.element('#wrapper').toggleClass('toggled');
        $scope.chatStatus = angular.element('#wrapper').hasClass('toggled');
      }

      $rootScope.dialpadNumber = '';
      console.debug('Redirecting to dialpad page.');
      $location.path('/dialpad');

      try {
        $rootScope.$digest();
      } catch (e) {
        console.log('not digest');
      }
    });

    $rootScope.$on('page.incall', function(event, data) {
      prompt({
        title: 'Oops, Active Call in Course.',
        message: 'It seems you were in a call before leaving the last time. Wanna go back to that?'
      }).then(function() {
        verto.changeData(angular.fromJson(storage.data.verto));
        console.log('redirect to incall page');
        $location.path('/incall');
      }, function() {
        storage.data.userStatus = 'connecting';
        verto.hangup();
      });
    });

    $rootScope.callActive = function(data) {
      verto.data.mutedMic = storage.data.mutedMic;
      verto.data.mutedVideo = storage.data.mutedVideo;

      if (!storage.data.cur_call) {
        storage.data.call_start = new Date();
      }
      storage.data.userStatus = 'connected';
      var call_start = new Date(storage.data.call_start);
      $rootScope.start_time = call_start;

      $timeout(function() {
        $scope.$broadcast('timer-start');
      });
      myVideo.play();
      storage.data.calling = false;

      storage.data.cur_call = 1;
    };

    $rootScope.$on('call.active', function(event, data) {
      $rootScope.callActive(data);
    });

    $rootScope.$on('call.calling', function(event, data) {
      storage.data.calling = true;
    });

    $rootScope.$on('call.incoming', function(event, data) {
      console.log('Incoming call from: ' + data);

      storage.data.cur_call = 0;
      $scope.incomingCall = true;
      storage.data.videoCall = false;
      storage.data.mutedVideo = false;
      storage.data.mutedMic = false;

      prompt({
        title: 'Incoming Call',
        message: 'from ' + data
      }).then(function() {
        var call_start = new Date(storage.data.call_start);
        $rootScope.start_time = call_start;
        console.log($rootScope.start_time);

        $scope.answerCall();
        storage.data.called_number = data;

        storage.data.call_history.unshift({
          'number': data,
          'direction': 'inbound',
          'status': true,
          'call_start': Date()
        });
        $location.path('/incall');
      }, function() {
        $scope.declineCall();
        storage.data.call_history.unshift({
          'number': data,
          'direction': 'inbound',
          'status': false,
          'call_start': Date()
        });
      });
    });

    $scope.hold = function() {
      storage.data.onHold = !storage.data.onHold;
      verto.data.call.toggleHold();
    };

    /**
     * Hangup the current call.
     */
    $scope.hangup = function() {
      if (!verto.data.call) {
        toastr.warning('There is no call to hangup.');
        $location.path('/dialpad');
      }

      //var hangupCallback = function(v, hangup) {
      //  if (hangup) {
      //    $location.path('/dialpad');
      //  } else {
      //    console.debug('The call could not be hangup.');
      //  }
      //};
      //
      //verto.hangup(hangupCallback);

      verto.hangup();
    };

    $scope.answerCall = function() {
      storage.data.onHold = false;

      verto.data.call.answer({
        useStereo: verto.data.useStereo,
        useCamera: verto.data.useCamera,
        useMic: verto.data.useMic,
        callee_id_name: verto.data.name,
        callee_id_number: verto.data.login
      });


      $location.path('/incall');
    };

    $scope.declineCall = function() {
      $scope.hangup();
      $scope.incomingCall = false;
    };

    $scope.screenshare = function() {
      if (verto.data.shareCall) {
        verto.screenshareHangup();
        return false;
      }
      verto.screenshare(storage.data.called_number);
    };

    $scope.play = function() {
      var file = $scope.promptInput('Please, enter filename', '', 'File',
        function(file) {
          verto.data.conf.play(file);
          console.log('play file :', file);
        });

    };

    $scope.stop = function() {
      verto.data.conf.stop();
    };

    $scope.record = function() {
      var file = $scope.promptInput('Please, enter filename', '', 'File',
        function(file) {
          verto.data.conf.record(file);
          console.log('recording file :', file);
        });
    };

    $scope.stopRecord = function() {
      verto.data.conf.stopRecord();
    };

    $scope.snapshot = function() {
      var file = $scope.promptInput('Please, enter filename', '', 'File',
        function(file) {
          verto.data.conf.snapshot(file);
          console.log('snapshot file :', file);
        });
    };


  }
]);


vertoControllers.controller('ChatController', ['$scope', '$rootScope', '$http',
  '$location', '$anchorScroll', '$timeout', 'verto',
  function($scope, $rootScope, $http, $location, $anchorScroll, $timeout,
    verto) {
    console.debug('Executing ChatController.');

    function scrollToChatBottom() {
      // Going to the bottom of chat messages.
      var obj = document.querySelector('.chat-messages');
      obj.scrollTop = obj.scrollHeight;
      //var chat_messages_top = jQuery('.chat-messages').scrollTop();
      //var marker_position = jQuery('#chat-message-bottom').position().top;
      //jQuery('.chat-messages').scrollTop(chat_messages_top + marker_position);
    }

    var CLEAN_MESSAGE = '';

    function clearConferenceChat() {
      $scope.members = [];
      $scope.messages = [];
      $scope.message = CLEAN_MESSAGE;
    }
    clearConferenceChat();

    $scope.$watch('activePane', function() {
      if ($scope.activePane == 'chat') {
        $rootScope.chat_counter = 0;
      }
      $rootScope.activePane = $scope.activePane;
    });

    $rootScope.$on('chat.newMessage', function(event, data) {
      data.created_at = new Date();
      console.log('chat.newMessage', data);
      $scope.$apply(function() {
        $scope.messages.push(data);
        if (data.from != verto.data.name && (!$scope.chatStatus ||
            $scope.activePane != 'chat')) {
          ++$rootScope.chat_counter;
        }
        $timeout(function() {
          scrollToChatBottom();
        }, 300);
      });
    });

    function findMemberByUUID(uuid) {
      var found = false;
      for (var idx in $scope.members) {
        var member = $scope.members[idx];
        if (member.uuid == uuid) {
          found = true;
          break;
        }
      }
      if (found) {
        return idx;
      } else {
        return -1;
      }
    }

    function translateMember(member) {
      return {
        'uuid': member[0],
        'id': member[1][0],
        'number': member[1][1],
        'name': member[1][2],
        'codec': member[1][3],
        'status': JSON.parse(member[1][4])
      };
    }

    function addMember(member) {
      $scope.members.push(translateMember(member));
    }

    $rootScope.$on('members.boot', function(event, members) {
      console.log('members.boot', event, members);
      $scope.$apply(function() {
        clearConferenceChat();
        for (var idx in members) {
          var member = members[idx];
          addMember(member);
          console.log($scope.members);
        }
      })
    });

    $rootScope.$on('members.add', function(event, member) {
      $scope.$apply(function() {
        addMember(member);
      });
    });

    $rootScope.$on('members.del', function(event, uuid) {
      $scope.$apply(function() {
        var memberIdx = findMemberByUUID(uuid);
        if (memberIdx != -1) {
          // Removing the member.
          $scope.members.splice(memberIdx, 1);
        }
      });
    });

    $rootScope.$on('members.update', function(event, member) {
      member = translateMember(member);
      var memberIdx = findMemberByUUID(member.uuid);
      if (memberIdx < 0) {
        console.log('Didn\'t find the member uuid ' + member.uuid);
      } else {
        $scope.$apply(function() {
          console.log('Updating', memberIdx, ' <', $scope.members[memberIdx],
            '> with <', member, '>');
          angular.extend($scope.members[memberIdx], member);
        });
      }
    });

    $rootScope.$on('members.clear', function(event) {
      $scope.$apply(function() {
        clearConferenceChat();
        $scope.closeChat();
      });
    });

    /**
     * Public methods.
     */
    $scope.send = function() {
      verto.sendMessage($scope.message, function() {
        $scope.message = CLEAN_MESSAGE;
      });
    };

    // Participants moderation.
    $scope.confKick = function(memberID) {
      console.log('$scope.confKick');
      verto.data.conf.kick(memberID);
    };

    $scope.confMuteMic = function(memberID) {
      console.log('$scope.confMuteMic');
      verto.data.conf.muteMic(memberID);
    };

    $scope.confMuteVideo = function(memberID) {
      console.log('$scope.confMuteVideo');
      verto.data.conf.muteVideo(memberID);
    };

    $scope.confPresenter = function(memberID) {
      console.log('$scope.confPresenter');
      verto.data.conf.presenter(memberID);
    };

    $scope.confVideoFloor = function(memberID) {
      console.log('$scope.confVideoFloor');
      verto.data.conf.videoFloor(memberID);
    };

    $scope.confBanner = function(memberID) {
      console.log('$scope.confBanner');
      var text = 'New Banner';
      verto.data.conf.banner(memberID, text);
    };

    $scope.confVolumeDown = function(memberID) {
      console.log('$scope.confVolumeDown');
      verto.data.conf.volumeDown(memberID);
    };

    $scope.confVolumeUp = function(memberID) {
      console.log('$scope.confVolumeUp');
      verto.data.conf.volumeUp(memberID);
    };

    $scope.confTransfer = function(memberID) {
      console.log('$scope.confTransfer');
      var exten = '1800';
      verto.data.conf.transfer(memberID, exten);
    };
  }
]);


vertoControllers.controller('MenuController', ['$scope', '$http', '$location',
  'verto', 'storage',
  function($scope, $http, $location, verto, storage) {
    console.debug('Executing MenuController.');
  }
]);


vertoControllers.controller('ModalSettingsController', ['$scope', '$http',
  '$location', '$modalInstance', 'verto', 'storage',
  function($scope, $http, $location, $modalInstance, verto, storage) {
    console.debug('Executing ModalSettingsController.');

    $scope.verto = verto;
    $scope.storage = storage;

    $scope.ok = function() {
      $modalInstance.close('Ok.');
      storage.data.verto = angular.toJson($scope.verto);
      verto.changeData($scope.verto);
    };

    $scope.cancel = function() {
      $modalInstance.dismiss('cancel');
    };

    $scope.refreshDeviceList = function() {
      verto.refreshDevices();
    }
  }
]);

vertoControllers.controller('ModalLoginInformationController', ['$scope',
  '$http', '$location', '$modalInstance', 'verto', 'storage',
  function($scope, $http, $location, $modalInstance, verto, storage) {
    console.debug('Executing ModalLoginInformationController.');

    $scope.verto = verto;
    $scope.storage = storage;

    $scope.ok = function() {
      $modalInstance.close('Ok.');
    };

    $scope.cancel = function() {
      $modalInstance.dismiss('cancel');
    };

  }
]);

vertoControllers.controller('LoginController', ['$scope', '$http', '$location',
  'verto',
  function($scope, $http, $location, verto) {
    $scope.checkBrowser();

    /**
     * using stored data (localStorage) for logon
     */
    verto.data.name = $scope.storage.data.name;
    verto.data.email = $scope.storage.data.email;
    if ($scope.storage.data.login != '' && $scope.storage.data.password != '') {
      verto.data.login = $scope.storage.data.login;
      verto.data.password = $scope.storage.data.password;
    }

    console.debug('Executing LoginController.');
  }
]);


vertoControllers.controller('DialPadController', ['$rootScope', '$scope',
  '$http', '$location', 'toastr', 'verto', 'storage',
  function($rootScope, $scope, $http, $location, toastr, verto, storage) {
    console.debug('Executing DialPadController.');

    $scope.checkBrowser();
    storage.data.videoCall = false;
    storage.data.userStatus = 'connecting';
    storage.data.calling = false;

    /**
     * fill dialpad via querystring [?autocall=\d+]
     */
    if ($location.search().autocall) {
      $rootScope.dialpadNumber = $location.search().autocall;
    }

    /**
     * used to bind click on number in call history to fill dialpad
     * 'cause inside a ng-repeat the angular isnt in ctrl scope
     */
    $scope.fillDialpadNumber = function(number) {
      $rootScope.dialpadNumber = number;
    };

    $rootScope.transfer = function() {
      if (!$rootScope.dialpadNumber) {
        return false;
      }
      verto.data.call.transfer($rootScope.dialpadNumber);
    };

    /**
     * Call to the number in the $rootScope.dialpadNumber.
     */
    $rootScope.call = function(extension) {
      storage.data.onHold = false;
      storage.data.cur_call = 0;
      $rootScope.dialpadNumber = extension;
      if (!$rootScope.dialpadNumber && storage.data.called_number) {
        $rootScope.dialpadNumber = storage.data.called_number;
        return false;
      } else if (!$rootScope.dialpadNumber && !storage.data.called_number) {
        toastr.warning('Enter an extension, please.');
        return false;
      }

      if (verto.data.call) {
        console.debug('A call is already in progress.');
        return false;
      }

      storage.data.mutedVideo = false;
      storage.data.mutedMic = false;

      storage.data.videoCall = false;
      verto.call($rootScope.dialpadNumber);

      storage.data.called_number = $rootScope.dialpadNumber;
      storage.data.call_history.unshift({
        'number': $rootScope.dialpadNumber,
        'direction': 'outbound',
        'call_start': Date()
      });
      $location.path('/incall');
    }
  }
]);


vertoControllers.controller('InCallController', ['$rootScope', '$scope',
  '$http', '$location', '$modal', '$timeout', 'toastr', 'verto', 'storage', 'prompt', 'Fullscreen',
  function($rootScope, $scope, $http, $location, $modal, $timeout, toatr,
    verto, storage, prompt, Fullscreen) {

    console.debug('Executing InCallController.');
    $scope.layout = null;
    $scope.checkBrowser();
    $rootScope.dialpadNumber = '';
    $scope.callTemplate = 'partials/phone_call.html';
    $scope.dialpadTemplate = '';
    $scope.incall = true;


    if (storage.data.videoCall) {
      $scope.callTemplate = 'partials/video_call.html';
    }

    $rootScope.$on('call.video', function(event, data) {
      $timeout(function() {
        $scope.callTemplate = 'partials/video_call.html';
      });
    });

    /**
     * toggle dialpad in incall page
     */
    $scope.toggleDialpad = function() {
      $scope.openModal('partials/dialpad_widget.html',
        'ModalDialpadController');

      /*
      if(!$scope.dialpadTemplate) {
        $scope.dialpadTemplate = 'partials/dialpad_widget.html';
      } else {
        $scope.dialpadTemplate = '';
      }
      */
    }

    /**
     * TODO: useless?
     */
    $scope.videoCall = function() {
      prompt({
        title: 'Would you like to activate video for this call?',
        message: 'Video will be active during the next calls.'
      }).then(function() {
        storage.data.videoCall = true;
        $scope.callTemplate = 'partials/video_call.html';
      });
    };

    $scope.cbMuteVideo = function(event, data) {
      storage.data.mutedVideo = !storage.data.mutedVideo;
    }

    $scope.cbMuteMic = function(event, data) {
      storage.data.mutedMic = !storage.data.mutedMic;
    }

    $scope.confChangeVideoLayout = function(layout) {
      verto.data.conf.setVideoLayout(layout);
    };

    $scope.muteMic = verto.muteMic;
    $scope.muteVideo = verto.muteVideo;

    $timeout(function() {
      console.log('broadcast time-start incall');
      $scope.$broadcast('timer-start');
    }, 1000);

  }
]);

vertoControllers.controller('ModalDialpadController', ['$scope',
  '$modalInstance',
  function($scope, $modalInstance) {

    $scope.ok = function() {
      $modalInstance.close('Ok.');
    };

    $scope.cancel = function() {
      $modalInstance.dismiss('cancel');
    };

  }
]);

vertoControllers.controller('BrowserUpgradeController', ['$scope', '$http',
  '$location', 'verto', 'storage', 'Fullscreen',
  function($scope, $http, $location, verto, storage, Fullscreen) {
    console.debug('Executing BrowserUpgradeController.');

  }
]);

vertoControllers.controller('ContributorsController', ['$scope', '$http',
  'toastr',
  function($scope, $http, toastr) {
    $http.get(window.location.pathname + '/contributors.txt')
      .success(function(data) {

        var contributors = [];

        angular.forEach(data, function(value, key) {
          var re = /(.*) <(.*)>/;
          var name = value.replace(re, "$1");
          var email = value.replace(re, "$2");

          this.push({
            'name': name,
            'email': email
          });
        }, contributors);

        $scope.contributors = contributors;
      })
      .error(function() {
        toastr.error('contributors not found.');
      });
  }
]);
