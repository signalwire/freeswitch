(function() {
  'use strict';

  angular
  .module('vertoControllers')
  .controller('ChatController', ['$scope', '$rootScope', '$http',
    '$location', '$anchorScroll', '$timeout', 'verto', 'prompt',
    function($scope, $rootScope, $http, $location, $anchorScroll, $timeout,
      verto, prompt) {
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
          if (data.from != verto.data.name &&
              (!$scope.chatStatus && $scope.activePane != 'chat')) {
            ++$rootScope.chat_counter;
          }
          $timeout(function() {
            scrollToChatBottom();
          }, 300);
        });
      });

      $rootScope.$on('call.conference', function(event, data) {
        $timeout(function() {
          $scope.conf = verto.data.conf.params.laData;
        });
      });

      $rootScope.$on('changedVideoLayout', function(event, layout) {
        $scope.resIDs = getResByLayout(layout);

        // remove resIDs param to clear every members resID.
        // passing $scope.resIDs results in preserving resIDs compatible
        // with the current layout
        clearMembersResID($scope.resIDs);
      });

      $rootScope.$on('conference.canvasInfo', function(event, data) {
        $scope.currentLayout = data[0].layoutName;
        $scope.resIDs = getResByLayout($scope.currentLayout);
      });

      function getResByLayout(layout) {
        var layoutsData = verto.data.confLayoutsData;
        for (var i = 0; i < layoutsData.length; i++) {
          if (layoutsData[i].name === layout) {
            return layoutsData[i].resIDS;
          }
        }
      }

      // @preserve - a array of values to be preserved
      function clearMembersResID(preserve) {
        $scope.members.forEach(function(member) {
          var resID = member.status.video.reservationID;
          console.debug("resID to clear: " + resID);
          if (resID && preserve && preserve.indexOf(resID) !== -1) return;
          if (resID){
            console.debug("clearing resid [" + resID + "] from [" + member.id + "]");
            $scope.confResID(member.id, resID);
          }
        });
      };

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
          'status': JSON.parse(member[1][4]),
          'email': member[1][5].email
        };
      }

      function addMember(member) {
        $scope.members.push(translateMember(member));
      }

      $rootScope.$on('members.boot', function(event, members) {
        $scope.$apply(function() {
          clearConferenceChat();
          for (var idx in members) {
            var member = members[idx];
            addMember(member);
          }
        })
      });

      $rootScope.$on('members.add', function(event, member) {
        $scope.$apply(function() {
          addMember(member);
        });
      });

      $rootScope.$on('members.del', function(event, uuid) {
        if ($rootScope.watcher && $rootScope.master === uuid) {
          verto.hangup();
          window.close();
        }

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
            // console.log('Updating', memberIdx, ' <', $scope.members[memberIdx],
              // '> with <', member, '>');
            // Checking if it's me
            if (parseInt(member.id) == parseInt(verto.data.conferenceMemberID)) {
              verto.data.mutedMic = member.status.audio.muted;
              verto.data.mutedVideo = member.status.video.muted;
			  verto.data.call.setMute(member.status.audio.muted ? "off" : "on");
			  verto.data.call.setVideoMute(member.status.video.muted ? "off" : "on");
            }
            angular.extend($scope.members[memberIdx], member);
          });
        }
      });

      $rootScope.$on('members.clear', function(event) {
        $scope.$applyAsync(function() {
          clearConferenceChat();
          $scope.closeChat();
        });
      });

      /**
       * Public methods.
       */

      $scope.toggleModMenu = function(index) {
        if (verto.data.confRole != 'moderator') return;
        $scope.openId = $scope.openId == index ? null : index;
      };

      $scope.send = function() {
        // Only conferencing chat is supported for now
        // but still calling method with the conference prefix
        // so we know that explicitly.
        verto.sendConferenceChat($scope.message);
        $scope.message = CLEAN_MESSAGE;
      };

      // Participants moderation.
      $scope.confKick = function(memberID) {
        console.log('$scope.confKick');
        verto.data.conf.kick(memberID);
      };

      $scope.confMuteMic = function(memberID) {
        if(verto.data.confRole == 'moderator') {
          console.log('$scope.confMuteMic');
          verto.data.conf.muteMic(memberID);
        }
      };

      $scope.confMuteVideo = function(memberID) {
        if(verto.data.confRole == 'moderator') {
          console.log('$scope.confMuteVideo');
          verto.data.conf.muteVideo(memberID);
        }
      };

      $scope.confPresenter = function(memberID) {
        console.log('$scope.confPresenter');
        verto.data.conf.presenter(memberID);
      };

      $scope.confResID = function(memberID, resID) {
        console.log('Set', memberID, 'to', resID);
        verto.setResevartionId(memberID, resID);
      };

      $scope.confVideoFloor = function(memberID) {
        console.log('$scope.confVideoFloor');
        verto.data.conf.videoFloor(memberID);
      };

      $scope.confBanner = function(memberID) {
        console.log('$scope.confBanner');

        prompt({
          title: 'Please insert the banner text',
          input: true,
          label: '',
          value: '',
        }).then(function(text) {
          if (text) {
            verto.data.conf.banner(memberID, text);
          }
        });
      };

      $scope.confCanvasIn = function(memberID, canvasID) {
        if (canvasID) {
          verto.setCanvasIn(memberID, canvasID);
          return;
        }

        shortPrompt('Please insert the Canvas Id', function(canvasID) {
          console.log(memberID, canvasID);
          verto.setCanvasIn(memberID, canvasID);
        });

      };

      $scope.confCanvasOut = function(memberID, canvasID) {
        if (canvasID) {
          verto.setCanvasOut(memberID, canvasID);
          return;
        }

        shortPrompt('Please insert the Canvas Id', function(canvasID) {
          verto.setCanvasOut(memberID, canvasID);
        });
      };

      $scope.confLayer = function(memberID, canvasID) {
        if (canvasID) {
          verto.setLayer(memberID, canvasID);
          return;
        }

        shortPrompt('Please insert the Layer', function(canvasID) {
          verto.setLayer(memberID, canvasID);
        });
      };

      $scope.confResetBanner = function(memberID) {
        console.log('$scope.confResetBanner');
        var text = 'reset';
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

      $scope.confGainDown = function(memberID) {
        console.log('$scope.confGainDown');
        verto.data.conf.gainDown(memberID);
      };

      $scope.confGainUp = function(memberID) {
        console.log('$scope.confGainUp');
        verto.data.conf.gainUp(memberID);
      };

      $scope.confTransfer = function(memberID) {
        console.log('$scope.confTransfer');
        prompt({
          title: 'Transfer party?',
          message: 'To what destination would you like to transfer this call?',
          input: true,
          label: 'Destination',
          value: '',
        }).then(function(exten) {
          if (exten) {
            verto.data.conf.transfer(memberID, exten);
          }
        });
      };

      function shortPrompt(text, cb) {
        prompt({
          title: text,
          input: true,
          label: '',
          value: '',
        }).then(function(val) {
          if (val && cb) {
            cb(val);
          }
        });
      }
    }
  ]);

})();
