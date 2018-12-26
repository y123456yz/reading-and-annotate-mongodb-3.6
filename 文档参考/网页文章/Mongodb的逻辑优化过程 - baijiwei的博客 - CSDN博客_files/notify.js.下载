(function(window){
 /**
  * @brief Notifier
  *
  * @param type 1. 一直提示 2，更新提示内容  3， 控制显示上限。超过个数删除最早的  4，超时更新
  * @param param  本参数在type为3，4时才有效， type为3表示可以最多显示通知的个数  4， 表示多少秒后删除了
  *
  * @return
  */
    function Notifier() {};


    window.Notifier = Notifier;


    type = 1;
    queue = [];
    t = 5;
    c = 3;
    _notifier = undefined ;

    if(undefined !==  window.webkitNotifications) {
      _notifier = window.webkitNotifications;

    }else if (undefined !== window.Notification)  {
      _notifier = window.Notification;
    } else {
      // console.log('error not found notification!')
    }

    window.Notifier.ModelAll = function() {
        type = 1;
    }

    window.Notifier.ModelUpdate =  function() {
        type = 2;
    }

    window.Notifier.ModelCount = function(ct) {
        if(ct !== undefined) c = ct;
        type = 3;
    }

    window.Notifier.ModelTimeout = function(timeout) {
        if(timeout !== undefined) t = timeout;
        type = 4;
    }



    window.Notifier.HasSupport = function() {
       if(undefined === _notifier) {
          return false;
       }

       return true;
    }

    window.Notifier.GetPermission = function() {
        return _notifier.checkPermission();
    }

    window.Notifier.IsGetPermission = function() {
        return (_notifier === 0);
    }

    window.Notifier.Disable = function() {
        return (_notifier.checkPermission === 2);
    }

    window.Notifier.RequestPermission = function(cb) {
      _notifier.requestPermission(function() {
            if(cb) {cb(_notifier.checkPermission() == 0)}
        });
    }



    //type = 1;关闭上一个
    window.Notifier.Close = function(type) {
        if(type = 1) {
            tmp = queue.pop();
        } else {
          tmp = queue.shift();
        }
        _closeItem(tmp);
    }

    window.Notifier.ClosePre = function () {
      tmp = queue.pop();
      _closeItem(tmp);
    }

    window.Notifier.CloseLast = function () {
      tmp = queue.shift();
      _closeItem(tmp);
    }

    window.Notifier.CloseAll = function() {
        while(queue.length > 0) {
          var tmp =  queue.shift();
          _closeItem(tmp);
        }
    }


    window.Notifier.Notify = function(icon, title, body) {
      if (this.IsGetPermission() == 0) {


        var popup = _createNotificationAndShow(icon, title, body);
        if(undefined == popup) {
          return false;
        }

        switch(type) {
          case 2:
            if(queue.length > 0) {
              tmp = queue.pop();
              _closeItem(tmp);
            }
            break;
          case 3:
            while(queue.length >= c) {
              tmp = queue.shift();
              _closeItem(tmp);
            }
            break;
          case 4:
            setTimeout(function(){_closeItem(popup);},  t*1000);
            break;
        }

        var q = queue;
        popup.onclose = function(){
            var cur = q.indexOf(popup);
            if(cur >= 0) {
                q.splice(cur, 1);
            }
        };


        popup.onclick = function(){};

        queue.push(popup);
        return true;
      } else {
		    RequestPermission();

	    }

      return false;
    }

    function _createNotificationAndShow(icon, title, body) {
      if(undefined != window.webkitNotifications && _notifier.name ===  window.webkitNotifications.name) {
        var n =  _notifier.createNotification(icon, title, body);
        n.show();
        return n;

      }else if (undefined !== window.Notification && _notifier.name ===  window.Notification.name)  {
        return  new _notifier(title, {icon:icon, body: body});
      } else {
        // console.log('error not found notification!')
        // alert(title +"\n\n"+body);
        return undefined;
      }
    }

    function _closeItem(n) {
      if(undefined == n) {
        return
      }
      if(n.cancel) {
        n.cancel();
      } else {
        n.close();
      }
    }




})(window);

