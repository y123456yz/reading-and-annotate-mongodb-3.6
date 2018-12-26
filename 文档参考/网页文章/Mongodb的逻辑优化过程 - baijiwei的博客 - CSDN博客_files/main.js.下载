'use strict';
(function(){
  const applicationServerPublicKey = 'BCYaMwiS92AJlv9Eg2YXSFwuI3ppbydkz31gOI5NS7YtOp05n7qUHEyb_iijzQcjgWqrsGSj2K18F21G9DYL4-U';
  let isSubscribed = false;
  let swRegistration = null;
  var linkUrl = ''

  function getBrowserInfo() {
    var agent = navigator.userAgent.toLowerCase();
    var regStr_ie = /msie [\d.]+;/gi;
    var regStr_ff = /firefox\/[\d.]+/gi
    var regStr_chrome = /chrome\/[\d.]+/gi;
    var regStr_saf = /safari\/[\d.]+/gi;
    var isIE = agent.indexOf("compatible") > -1 && agent.indexOf("msie" > -1); //判断是否IE<11浏览器
    var isEdge = agent.indexOf("edge") > -1 && !isIE; //判断是否IE的Edge浏览器
    var isIE11 = agent.indexOf('trident') > -1 && agent.indexOf("rv:11.0") > -1;
    if (isIE) {
      var reIE = new RegExp("msie (\\d+\\.\\d+);");
      reIE.test(agent);
      var fIEVersion = parseFloat(RegExp["$1"]);
      if (fIEVersion == 7) {
        return "IE/7";
      } else if (fIEVersion == 8) {
        return "IE/8";
      } else if (fIEVersion == 9) {
        return "IE/9";
      } else if (fIEVersion == 10) {
        return "IE/10";
      }
    } //isIE end
    if (isIE11) {
      return "IE/11";
    }
    //firefox
    if (agent.indexOf("firefox") > 0) {
      return agent.match(regStr_ff);
    }
    //Safari
    if (agent.indexOf("safari") > 0 && agent.indexOf("chrome") < 0) {
      return agent.match(regStr_saf);
    }
    //Chrome
    if (agent.indexOf("chrome") > 0) {
      return agent.match(regStr_chrome);
    }
  }

  function urlB64ToUint8Array(base64String) {
    const padding = '='.repeat((4 - base64String.length % 4) % 4);
    const base64 = (base64String + padding)
      .replace(/\-/g, '+')
      .replace(/_/g, '/');

    const rawData = window.atob(base64);
    const outputArray = new Uint8Array(rawData.length);

    for (let i = 0; i < rawData.length; ++i) {
      outputArray[i] = rawData.charCodeAt(i);
    }
    return outputArray;
  }

  if ('serviceWorker' in navigator && 'PushManager' in window) {
    var _hmt = window._hmt || [];
    var hasSub = getCookie('hasSub')
    // 判断域名的操作 如果域名是blog 和download
    var flagBox1 = window.location.host.indexOf('blog')>=0 ?'true':'false'
    var flagBox2 = window.location.host.indexOf('download')>=0 ?'true':'false'
    var fs = window.RequestFileSystem || window.webkitRequestFileSystem;
    if (!fs) {
      console.log("check failed");
      handleRight()
    } else {
      fs(window.TEMPORARY,100,function(){
        //console.log('非隐身模式');
        handleRight()
      },function(){
        //console.log('隐身模式');
        return false
      });
    }
  } else {
    //火狐浏览器隐身模式下 serviceWorker属性都不支持 safari不支持 可以全站推送 走此逻辑
    if ('serviceWorker' in navigator) {
      // 注册service worker
      navigator.serviceWorker.register('/sw.js').then(function (registration) {
        return askPermission();
      }).then(function () {
        console.log('safari授权成功了====')
        var subscription = '-'
        handleAjax(subscription)
        // workOrderSync()
      })
    }

  }
  //
  function askPermission() {
    return new Promise(function (resolve, reject) {
      var permissionResult = Notification.requestPermission(function (result) {
        resolve(result);
      });

      if (permissionResult) {
        permissionResult.then(resolve, reject);
      }
    }).then(function (permissionResult) {
      if (permissionResult !== 'granted') {
        throw new Error('We weren\'t granted permission.');
      }
    });
  }
// 操作权限
  function handleRight() {
     if((hasSub!=='true'&&flagBox1 === 'true')||(hasSub!=='true'&&flagBox2 === 'true')){
    // 没有授权的操作
    // if((hasSub!=='true')||(hasSub!=='true')){
      var Notification = window.Notification || window.mozNotification || window.webkitNotification;
      navigator.serviceWorker.register('/sw.js')
        .then(function(swReg) {
          //console.log('注册成功了')
          swRegistration = swReg;
         // workOrderSync(swRegistration)
          // sw注册后调用检查用户是否订阅通知的函数

          initialiseUI();
          Notification.requestPermission(function(status){
            if(status==='denied'&& getCookie('firstDie')!=='1'){
              // 隐身模式下不弹框 默认直接拒绝授权
              // && getCookie('firstDie')!=='1'
              //用户拒绝授权 再次选择 弹出授权框拒绝才能再次记录
              _hmt.push(['_trackEvent', '弹出框', '拒绝授权', '', 'rejectRightBox'])
              // console.log('执行rejectRightBox')
              setCookie('firstDie','1',1)
            } else if(status==='granted'){
              // 用户同意授权
              // 授权过的就不再进入这个里面了
              //执行了授权 只能清缓存 弹出授权框允许才能再次记录
              //debugger
              subscribeUser()
            }
          })
        })
        .catch(function(error) {
          //console.log('隐身模式下注册不成功')
          var browser = getBrowserInfo()[0] || '';
          //console.log('browser',browser)
          _hmt.push(['_trackEvent', 'serviceWorker', '报错', '', 'serviceWorker报错'])
          _hmt.push(['_trackEvent', '浏览器版本精确版本', browser, '', '真实不能注册的浏览器版本'])
          // $.get('https://statistic.csdn.net/notification/error?brower='+browser)
        });
      // 可以手动获取权限 但是否去选择弹出是不知道的
      // navigator.serviceWorker.ready.then(function(swRegistration) {
      //   console.log('workOrderSync执行了')
      //   return swRegistration.sync.register("workOrderSync");
      // });
    }else{
      navigator.serviceWorker.ready.then(function(swRegistration) {
        // //console.log('刷新走了else====进入了workSync')
        // return swRegistration.sync.register("workOrderSync");
        // var options = {
        //   body: 'heihei',
        //   icon: 'img/logo.png',
        //   badge: 'img/badge.png'
        // };
        // var title = 'haha666';
        // swRegistration.showNotification(title, options);
       // workOrderSync(swRegistration)
      });
      // 注册过的和不是blog 或者 download 域名下如果接口数据有返回 的可以直接拿数据展示


    }
  }

  function workOrderSync(swRegistration) {
    var opts = {
      method:"POST",   //请求方法
      body:{},   //请求体
      headers: {
        'Accept': 'application/json',
        'Content-Type': 'application/json',
      },
      credentials: "include"

    }
    setInterval(function () {
      // https://test-notification.csdn.net/notifiction/fe/getbyusername
      fetchData(opts,swRegistration)
    }, 20000)
    fetchData(opts,swRegistration)
  }
  function fetchData(opts,swRegistration){
    fetch("https://test-notify.csdn.net/notifiction/fe/getbyusername",opts).then(res=> {
      return res.json()
    }).then(res=> {
      //console.log('res1===',res);
      var jsonData = res.data
      //console.log('jsonData===',jsonData);
      var title = jsonData.title;
      linkUrl = jsonData.url || ''
      var options = {
        body: jsonData.message,
        icon: 'img/logo.png',
        badge: 'img/badge.png',
        data:linkUrl
        // actions: [{
        //   action: '',
        //   title: ''
        // }]
      };
      var browser = getBrowserInfo()[0] || '';
      console.log('浏览器信息输出browser',browser)
      if(JSON.stringify(jsonData)!=='{}'&&res.status){
        if(browser&&browser.indexOf('safari/')>-1){
          var notification = new Notification(title, options);
          console.log('走了safari逻辑里面====',notification)
          notification.addEventListener('click', function (e) {
            window.location.href = linkUrl
          });
        } else {
          swRegistration.showNotification(title, options)
        }
        _hmt.push(['_trackEvent', '推送消息弹框', '消息', '', '展示推送弹框'])
      }
    }).catch(err=> {
      //console.log(err);
    })
  }
// 检查用户当前有没有订阅
  function initialiseUI() {
    swRegistration.pushManager.getSubscription()
      .then(function(subscription) {
        isSubscribed = !(subscription === null);
        if (isSubscribed) {
        } else {
        }
        updateBtn();
      });
  }
// cookie
  function setCookie(cname, cvalue, exdays) {
    var d = new Date();
    d.setTime(d.getTime() + (exdays * 24 * 60 * 60 * 1000));
    var expires = "expires=" + d.toUTCString();
    document.cookie = cname + "=" + cvalue + "; " + expires+";domain=csdn.net;path=/"
    // console.log(d)
  }


  //获取cookie
  function getCookie(cname) {
    var name = cname + "=";
    var ca = document.cookie.split(';');
    for(var i = 0; i < ca.length; i++) {
      var c = ca[i];
      while(c.charAt(0) == ' ') c = c.substring(1);
      if(c.indexOf(name) != -1) return c.substring(name.length, c.length);
    }
    return "";
  }
  function delCookie(name)
  {
    var exp = new Date();
    exp.setTime(exp.getTime() - 1);
    var cval=getCookie(name);
    if(cval!=null)
      document.cookie= name + "="+cval+";expires="+exp.toUTCString()+';domain=csdn.net;path=/';
  }
//启用我们的按钮，以及更改用户是否订阅的文本
  function updateBtn() {
    // 只执行一次 如何监听
    if (Notification.permission === 'denied') {
      return;
    }
    if(Notification.permission === 'default'){
      _hmt.push(['_trackEvent', '弹出框', '展示授权弹框', '', 'showRightBox'])
      //console.log('执行showRightBox')
      delCookie('firstDie')
      delCookie('hasSub')
    }
    if(Notification.permission === 'granted'){
    }
    if (isSubscribed) {
    } else {

    }
  }
  function subscribeUser() {
    const applicationServerKey = urlB64ToUint8Array(applicationServerPublicKey);
    // 传递公钥给sw服务器
    //debugger
    swRegistration.pushManager.subscribe({
      userVisibleOnly: true,
      // 默认允许订阅后发送通知
      applicationServerKey: applicationServerKey
    })
      .then(function(subscription) {
        // setCookie('firstEnter','1',1)
        handleAjax(subscription)
        _hmt.push(['_trackEvent', '弹出框', '允许授权', '', 'agreeRightBox'])
       // console.log('执行agreeRightBox2')
        isSubscribed = true
        //updateBtn()
      })
      .catch(function(err) {
        // updateBtn()
        _hmt.push(['_trackEvent', '请求接口', '失败', '', '接口报错'])
      });
  }
  function handleAjax(subscription){
    var xhr;
    if (window.XMLHttpRequest)
    {
      //  IE7+, Firefox, Chrome, Opera, Safari 浏览器执行代码
      xhr = new XMLHttpRequest();
    }
    else
    {
      // IE6, IE5 浏览器执行代码
      xhr = new ActiveXObject("Microsoft.XMLHTTP");
    }
//设置请求的类型及url
    // application/x-www-form-urlencoded
    xhr.open('post', 'https://gw.csdn.net/cui-service/v1/browse_info/save_browse_info' );
//发送请求
    //post请求一定要添加请求头才行不然会报错 open后send前
    xhr.setRequestHeader("Content-type","application/json");
    var jsonData = ''
    if(typeof(subscription) === 'string'){
      jsonData ={'subscription':subscription}
    } else {
      jsonData ={'subscription':JSON.stringify(subscription)}
    }
    xhr.withCredentials = true;
    xhr.send(JSON.stringify(jsonData));
    xhr.onreadystatechange = function () {
      // 这步为判断服务器是否正确响应
      if (xhr.readyState == 4 && xhr.status == 200) {
        //console.log(xhr.responseText);
        setCookie('hasSub',true,1)
      }
    };
  }
}(window))
