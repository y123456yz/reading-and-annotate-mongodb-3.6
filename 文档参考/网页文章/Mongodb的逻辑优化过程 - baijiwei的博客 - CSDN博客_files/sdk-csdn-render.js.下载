(function(window, document /*,undefined*/) {
  //2013.1.24 by 司徒正美
  function contains(parentEl, el, container) {
    // 第一个节点是否包含第二个节点
    //contains 方法支持情况：chrome+ firefox9+ ie5+, opera9.64+(估计从9.0+),safari5.1.7+
    if (parentEl == el) {
      return true;
    }
    if (!el || !el.nodeType || el.nodeType != 1) {
      return false;
    }
    if (parentEl.contains) {
      return parentEl.contains(el);
    }
    if (parentEl.compareDocumentPosition) {
      return !!(parentEl.compareDocumentPosition(el) & 16);
    }
    var prEl = el.parentNode;
    while (prEl && prEl != container) {
      if (prEl == parentEl) return true;
      prEl = prEl.parentNode;
    }
    return false;
  }

  /**
   * 转换obj为字符串param
   */
  function parseParam(params) {
    if (!params) return "";
    var sb = "",
      val;
    for (var key in params) {
      val = params[key];
      sb += "&" + key + "=" + encodeURIComponent(val);
    }
    return sb;
  }

  /**
   * 获取特殊类型的物料
   */
  function getCSDNAdsItem() {
    var adsItem = {
      item_id: "user_define",
      special_type: "csdn_net_alliance_ads",
      ads: 1
    };
    return adsItem;
  }
  /**
   * 获取当前年月日
   */
  function CurentTime() {
    var now = new Date();
    // var year = now.getFullYear(); //年
    var month = now.getMonth() + 1; //月
    var day = now.getDate(); //日
    return month + "月" + day + "日";
  }

  function randomStr(len) {
    var sb = "";
    var dict = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789abcdefghijklmnopqrstuvwxyz";
    for (var i = 0; i < len; ++i)
      sb += dict.charAt((Math.random() * dict.length) | 0);
    return sb;
  }

  function hrTime(x) {
    var date = new Date(x * 1000),
      it;
    var MM = (it = date.getMonth() + 1) < 10 ? "0" + it : it;
    var dd = (it = date.getDay()) < 10 ? "0" + it : it;
    var HH = (it = date.getHours()) < 10 ? "0" + it : it;
    var mm = (it = date.getMinutes()) < 10 ? "0" + it : it;
    var ss = (it = date.getSeconds()) < 10 ? "0" + it : it;
    return (
      date.getFullYear() + "-" + MM + "-" + dd + " " + HH + ":" + mm + ":" + ss
    );
  }

  function newXHR(stack) {
    var xhr = new XMLHttpRequest();
    if (!window.TINGYUN || !TINGYUN.createEvent) return xhr;
    var event = TINGYUN.createEvent({
      name: stack.join("_"),
      key: "b3d532c8-f6e2-4978-8b7f-31e7255c46e9"
    });
    xhr.addEventListener("error", function() {
      event.fail();
    });
    xhr.addEventListener("load", function() {
      event.end();
    });
    return xhr;
  }

  /**
   * action @param {string} 动作类型
   * sceneID @param {string} 场景ID
   * itemPfx @param {string} 场景前缀
   * itemID @param {string} 物料Id
   * context @param {string} 物料的上下文
   * isNetAllianceAds @param {boolean} 如果是网盟广告类型要添加额外字段
   */
  function action(action, sceneID, itemPfx, itemID, context) {
    var url =
        host +
        "/action/api/log?requestID=" +
        requestID +
        "&clientToken=" +
        clientToken,
      ref = {
        requestID: requestID,
        actionTime: Date.now(),
        action: action,
        sceneId: sceneID,
        userId: userID,
        itemId: itemID,
        context: context,
        itemSetId: "" + itemPfx,
        uuid_tt_dd: uuid_tt_dd
      };
    var xhr = newXHR(["p4sdk", "log", sceneID, itemPfx]);

    // 如果是网盟信息的话添加额外字段到ref当中作为标记
    if (itemID === "user_define") {
      (ref.specialType = "csdn_net_alliance_ads"), (ref.ads = 1);
    }

    xhr.open("POST", url);
    xhr.setRequestHeader("Content-Type", "text/plain");
    xhr.send(
      JSON.stringify({
        date: hrTime(ref.actionTime / 1000),
        actions: [ref]
      })
    );
  }

  if (window["p4sdk_singleton_main_render"]) return;
  window["p4sdk_singleton_main_render"] = true;

  var host = "https://nbrecsys.4paradigm.com";
  var uuid_tt_dd = (document.cookie.match(/\buuid_tt_dd=([^;]+)/) || [])[1];
  var clientToken = "1f9d3d10b0ab404e86c2e61a935d3888";
  var k = "paradigmLocalStorageUserIdKey";
  var userID = localStorage[k] || (localStorage[k] = randomStr(10));
  var requestID = randomStr(8);
  var seedItemID =
    (location.href.match(/\/article\/details\/(\d+)/) || [])[1] ||
    (typeof articleID !== "undefined" ? articleID : "");
  var itemTitle =
    (document.getElementsByClassName("title-article") &&
      document.getElementsByClassName("title-article")[0] &&
      document.getElementsByClassName("title-article")[0].innerHTML) ||
    "";
  var req = {
    itemID: seedItemID,
    uuid_tt_dd: uuid_tt_dd,
    itemTitle: itemTitle
  };
  var url =
    host +
    "/api/v0/recom/recall?requestID=" +
    requestID +
    "&userID=" +
    userID +
    "&sceneID=";
  action("detailPageShow", 788, 42, seedItemID);
  var dedup = {};

  var positionConfig = {
    T0: { sceneId: 420, itemSetId: 39 },
    T3: { sceneId: 34, itemSetId: 39 },
    T7: { sceneId: 750, itemSetId: 39 },
    T38: { sceneId: 3649, itemSetId: 1530 },
    T43: { sceneId: 3650, itemSetId: 1530 },
    T48: { sceneId: 3651, itemSetId: 1530 },
    S: { sceneId: 788, itemSetId: 42 }
  };
  /**
   * 判断遨游浏览器的Url
   * @param {*} item
   */
  function setAoYouUrl(item) {
    if (!item || item["item_id"] !== "aoyouliulanqi1") {
      return;
    }
    /** * 是否为mac系统（包含iphone手机） * */
    var isMac = /macintosh|mac os x/i.test(navigator.userAgent);
    var aoyouUrl = isMac
      ? "http://dl.maxthon.cn/mac5/Maxthon-r5.1.134.181219-csdn.dmg"
      : "http://mk.maxthon.cn/mx5/5chlcsdn201812v1/mx_5chlcsdn201812v1.exe";
    item["url"] = aoyouUrl;
    item["title"] = "<em>" + CurentTime() + "</em>" + item["title"];
  }
  // T位置的渲染
  function p4CSDNTBootstrap(csdnRender, div, position) {
    var currentSceneId = positionConfig[position].sceneId;
    var currentItemSetId = positionConfig[position].itemSetId;

    var xhr = newXHR(["p4sdk", "recall", currentSceneId]);
    xhr.open("POST", url + currentSceneId);
    xhr.addEventListener("load", function() {
      var raw = xhr.responseText;
      var json = JSON.parse(raw);
      var item = json[0];
      if (item) {
        if (dedup[item["item_id"]]) item = json[1];
        if (item) dedup[item["item_id"]] = 1;
      }
      // 处理遨游浏览器的Url
      setAoYouUrl(item);
      // 这里我们需要创造一个特殊的物料，用来标明物料类型为：csdn的网盟广告
      if (!item || item["item_id"] === "-1") item = getCSDNAdsItem();
      csdnRender(item, div, position);

      // 只有对用户可见的时候才上报
      setTimeout(scroll);
      window.addEventListener("scroll", scroll);
      function scroll() {
        if (!contains(document, div)) return;
        var rect = div.getBoundingClientRect();
        var x = (rect.left + rect.right) / 2,
          y = (rect.top + rect.bottom) / 2;
        if (
          x >= 0 &&
          x <= document.documentElement.clientWidth &&
          y >= 0 &&
          y <= document.documentElement.clientHeight
        ) {
          action(
            "show",
            currentSceneId,
            currentItemSetId,
            item["item_id"],
            item["context"]
          );
          window.removeEventListener("scroll", scroll);
        }
      }

      // 点击上报
      div.addEventListener("click", function() {
        action(
          "detailPageShow",
          currentSceneId,
          currentItemSetId,
          item["item_id"],
          item["context"]
        );
      });
    });
    xhr.send(JSON.stringify(req));
  }
  // 侧边栏位置的渲染
  function p4CSDNSBootstrap(csdnRender, div) {
    var currentSceneId = positionConfig["S"].sceneId;
    var currentItemSetId = positionConfig["S"].itemSetId;

    var xhr = newXHR(["p4sdk", "recall", currentSceneId]);
    xhr.open("POST", url + currentSceneId);
    xhr.addEventListener("load", function() {
      var raw = xhr.responseText;
      var json = JSON.parse(raw);
      // vip客户不显示广告
      var sideBarLength =
        isShowAds && json.length > 26 ? json.length + 4 : json.length;
      for (var i = 0; i < sideBarLength; i++) {
        (function(i) {
          var itemDom = document.createElement("li");
          itemDom.setAttribute("class", "right-item");
          itemDom.setAttribute("id", "para_render_s" + i);
          //vip客户不显示广告
          if (isShowAds && (i === 2 || i === 7 || i === 13 || i === 26)) {
            div.appendChild(itemDom);
            csdnRender(getCSDNAdsItem(), itemDom, "S" + (i + 1));
          } else {
            div.appendChild(itemDom);
            let indexNum = i;
            if (isShowAds) {
              //vip客户不显示广告
              if (i > 2 && i < 7) {
                indexNum = i - 1;
              } else if (i > 7 && i < 13) {
                indexNum = i - 2;
              } else if (i > 13 && i < 26) {
                indexNum = i - 3;
              } else if (i > 26) {
                indexNum = i - 4;
              }
            }
            var item = json[indexNum];
            itemDom.setAttribute(
              "data-track-click",
              '{"mod":"popu_652","con":",' +
                item["url"] +
                ",-,index_" +
                indexNum +
                '"}'
            );
            // 点击上报
            itemDom.addEventListener("click", function() {
              action(
                "detailPageShow",
                currentSceneId,
                currentItemSetId,
                item["item_id"],
                item["context"]
              );
            });
            csdnRender(item, itemDom, "S" + (i + 1));
          }
        })(i);
      }
      if (json && window.right_recommend) {
        window.right_recommend.openToolStyleChange();
      }

      // 只有对用户可见的时候才上报
      setTimeout(scroll);
      window.addEventListener("scroll", scroll);
      function scroll() {
        if (!contains(document, div)) return;
        var rect = div.getBoundingClientRect();
        var x = (rect.left + rect.right) / 2,
          y = (rect.top + rect.bottom) / 2;
        if (
          x >= 0 &&
          x <= document.documentElement.clientWidth &&
          y >= 0 &&
          y <= document.documentElement.clientHeight
        ) {
          for (var i = 0; i < json.length; i++) {
            var item = json[i];
            if (i === 2 || i === 7 || i === 13) {
            } else {
              action(
                "show",
                currentSceneId,
                currentItemSetId,
                item["item_id"],
                item["context"]
              );
            }
          }
          window.removeEventListener("scroll", scroll);
        }
      }
    });
    xhr.send(JSON.stringify(req));
  }
  function sampleBuildUrl(stage, itemID, url) {
    var sb =
      host +
      "/api/v0/csdn/sample-t0?stage=" +
      stage +
      "&requestID=" +
      requestID +
      "&userID=" +
      userID +
      "&itemID=" +
      itemID;
    if (url) sb += "&url=" + encodeURIComponent(url);
    return sb;
  }
  function htmlEncode(html) {
    //1.首先动态创建一个容器标签元素，如DIV
    var temp = document.createElement("div");
    //2.然后将要转换的字符串设置为这个元素的innerText(ie支持)或者textContent(火狐，google支持)
    temp.textContent != undefined
      ? (temp.textContent = html)
      : (temp.innerText = html);
    //3.最后返回这个元素的innerHTML，即得到经过HTML编码转换的字符串了
    var output = temp.innerHTML;
    temp = null;
    return output;
  }
  /**
   * 获取结果后的回调
   * @param {物料对象} item
   * @param {渲染的dom} renderParentDom
   * @param {位置是T0,T3,T7或侧边栏的位置S3,S8,S14} position
   */
  function renderCallBack(item, renderParentDom, position) {
    if (
      position === "T3" ||
      position === "T38" ||
      position === "T43" ||
      position === "T48"
    ) {
      addTPosStyle(item);
    }
    if (!judgeAdsMark(item.item_id, renderParentDom, position)) {
      render4P(item, renderParentDom, position);
    }
  }
  function render4P(item, renderParentDom, pos) {
    var config = { pos: pos, is_img: false };
    var itemDom = "";

    var showTime =
      item.publish_time && item.publish_time.length > 9
        ? item.publish_time.substring(5, 10)
        : "";
    if (pos === "T0") {
      addP0Style(item);
      var imgDom = "";
      itemDom = document.createElement("div");
      if (item.cover_url) {
        config.is_img = true;
        imgDom =
          '<div class="img-box float-left"><img src="' +
          item.cover_url +
          '" ></div>';
      }
      $(renderParentDom).addClass("isGreatIcon");
      itemDom.setAttribute("class", "_4paradigm_box clearfix T0");
      itemDom.setAttribute(
        "style",
        "background-color: #fff;margin-bottom: 1px;margin-top: 1px;padding: 16px 24px;"
      );
      itemDom.innerHTML =
        '<a href="' +
        item.url +
        '" target="_blank"><div class="content-box"><h4 class="text-truncate">' +
        item.title +
        '</h4><p class="content oneline">' +
        item.content +
        "</p></div>" +
        imgDom +
        "</a>";
    } else if (
      pos === "T3" ||
      pos === "T38" ||
      pos === "T43" ||
      pos === "T48"
    ) {
      var imgDom = "";
      itemDom = document.createElement("div");
      if (item.cover_url) {
        config.is_img = true;
        imgDom =
          '<div class="img-box float-left"><img style="width:96px;height:60px;border-radius: 4px;" src="' +
          item.cover_url +
          '" ></div>';
      }
      $(renderParentDom).addClass("isGreatIcon");
      itemDom.setAttribute("class", "_4paradigm_box  clearfix " + pos);
      itemDom.innerHTML =
        '<a href="' +
        item.url +
        '" target="_blank"><div class="content-box"><h4 class="text-truncate oneline">' +
        item.title +
        '</h4><p class="content oneline">' +
        item.content +
        "</p></div>" +
        imgDom +
        "</a>";
    } else if (pos === "T7") {
      itemDom = document.createElement("a");
      $(renderParentDom).addClass("isGreatIcon");
      itemDom.setAttribute("href", item.url);
      itemDom.setAttribute("target", "_blank");
      itemDom.innerHTML =
        '<div class="content-box"><h4>' +
        item.title +
        '</h4><div class="recommend_user_info row"><span class="read col-md-4">' +
        item.item_read_cnt +
        htmlEncode("次阅读") +
        '</span><span class="time col-md-4">' +
        showTime +
        "</span></div>";
    } else {
      itemDom = document.createElement("a");
      itemDom.setAttribute("href", item.url);
      itemDom.setAttribute("target", "_blank");
      // itemDom.innerHTML =
      //   '<div class="context-box ">' +
      //   '<div class="content clearfix ">' +
      //   '<h5 class=""' +
      //   'title="' +
      //   htmlEncode(item.title) +
      //   '">' +
      //   htmlEncode(item.title) +
      //   "</h5>" +
      //   '<span class="time">&nbsp;&nbsp;' +
      //   showTime +
      //   '</span><span class="read">' +
      //   '<svg class="icon no-active hover-hide" aria-hidden="true">' +
      //   '<use xlink:href="#csdnc-m-passwords-visible"></use></svg>' +
      //   item.item_read_cnt +
      //   "</span>" +
      //   "</div>" +
      //   "</div>";
      itemDom.innerHTML =
        '<div class="context-box ">' +
        '<div class="content clearfix ">' +
        '<h5 class=""' +
        'title="' +
        htmlEncode(item.title) +
        '">' +
        htmlEncode(item.title) +
        "</h5>" +
        '<span class="time">&nbsp;&nbsp;' +
        showTime +
        "</span>" +
        "</div>" +
        "</div>";
    }
    $(renderParentDom).append(itemDom);
  }
  // 增加T0位置的css
  function addP0Style(item) {
    var contentWidth = "84%";
    if (!item.cover_url) contentWidth = "100%";

    var t0CusStyle = [
      ".T0{",
      "background-color: #fff;",
      "margin-bottom: 1px;",
      "margin-top: 1px;",
      "padding: 16px 24px;",
      "} ",
      ".T0 .img-box img{",
      "width:96px;",
      "height:60px;",
      "margin-top: 6px;",
      "border-radius: 4px;",
      "}",
      ".T0 .content-box{ width: " +
        contentWidth +
        ";display:inline-block;margin-left: 8px;} ",
      ".T0 .content-box h4{",
      "font-size: 18px;",
      "color: #3C3C3C;",
      "line-height: 24px;",
      "margin-bottom:4px;",
      "} ",
      ".T0 .content-box h4:hover{",
      "color: #CA0C16;",
      "} ",
      ".T0 .content-box .content{",
      "font-size: 14px;",
      "color: #6B6B6B;",
      "}"
    ].join("");
    var style = document.createElement("style");
    style.type = "text/css";
    style.innerHTML = t0CusStyle;
    document.getElementsByTagName("HEAD")[0].appendChild(style);
  }
  // 增加T位置的css
  function addTPosStyle(item) {
    var style = document.createElement("style");
    style.type = "text/css";
    style.innerHTML =
      ".recommend-box ._4paradigm_box.T3 a .content-box{margin-left:0} .recommend-box ._4paradigm_box.T3 a .content-box h4{vertical-align:top}.recommend-box ._4paradigm_box.p4courset3_target {padding:0}.recommend-box ._4paradigm_box._paradigm_T_ads_render {padding:0} .recommend-box ._4paradigm_box._paradigm_T_ads_render a {width:100%;display:inline-block}";
    document.getElementsByTagName("HEAD")[0].appendChild(style);
  }

  /**
   * 判断是否是广告位若是则渲染百度广告
   * @param {物料id} itemId
   * @param {渲染dom} renderParentDom
   * @param {位置是T0,T3,T7或侧边栏的位置S3,S8,S14} position
   */
  function judgeAdsMark(itemId, renderParentDom, position) {
    if (itemId == adsMark) {
      renderSlotbydup(renderParentDom, position);
      return true;
    }
    return false;
  }
  /**
   * 渲染百度广告
   * @param {渲染位置的dom} renderParentDom
   * @param {位置是T0,T3,T7,T38,T43,T48或侧边栏的位置S3,S8,S14} position
   */
  function renderSlotbydup(renderParentDom, position) {
    if (position === "T0") {
      renderT0NEWSFEED(renderParentDom);
    } else if (
      position === "T3" ||
      position === "T38" ||
      position === "T43" ||
      position === "T48"
    ) {
      (function() {
        var scriptT3Dom = document.createElement("script");
        scriptT3Dom.id = "_paradigm_" + position + "_csdn_ads_script";
        $(renderParentDom).addClass(
          "_paradigm_" + position + "_csdn_ads_render"
        );
        scriptT3Dom.src =
          "https://nbrecsys.4paradigm.com/resource/cus/csdn/static/csdn-" +
          position +
          "-ads.js";
        renderParentDom.appendChild(scriptT3Dom);
      })();
    } else if (position === "T7") {
      renderParentDom.style.padding = "0";
      (function() {
        var scriptT7Dom = document.createElement("script");
        scriptT7Dom.id = "_paradigm_T7_csdn_ads_script";
        $(renderParentDom).addClass("_paradigm_T7_csdn_ads_render");
        scriptT7Dom.src =
          "https://nbrecsys.4paradigm.com/resource/cus/csdn/static/csdn-T7-ads.js";
        renderParentDom.appendChild(scriptT7Dom);
      })();
    } else if (
      position === "S3" ||
      position === "S8" ||
      position === "S14" ||
      position === "S27"
    ) {
      var positionUId = { S3: "u3580958", S8: "u3535678", S14: "u3535679" };
      (function() {
        var scriptSDom = document.createElement("script");
        scriptSDom.id = "_paradigm_" + position + "_csdn_ads_script";
        $(renderParentDom).addClass(
          "_paradigm_" + position + "_csdn_ads_render"
        );
        scriptSDom.src =
          "https://nbrecsys.4paradigm.com/resource/cus/csdn/static/csdn-" +
          position +
          "-ads.js";
        renderParentDom.appendChild(scriptSDom);
      })();
    }
  }
  /**
   * T0位置渲染广告
   * @param {渲染位置的dom} renderParentDom
   */
  function renderT0NEWSFEED(renderParentDom) {
    var e = 10;
    renderParentDom.id = "a_d_feed_" + e;
    var containerWidth = $("div.blog-content-box").outerWidth() - 40;
    "function" != typeof NEWS_FEED
      ? ""
      : NEWS_FEED({
          w: containerWidth,
          h: 80,
          showid: "ztvJV8",
          placeholderId: "a_d_feed_" + e,
          inject: "define",
          define: {
            imagePosition: "left",
            imageBorderRadius: 0,
            imageWidth: 90,
            imageHeight: 60,
            imageFill: "clip",
            displayImage: !0,
            displayTitle: !0,
            titleFontSize: 20,
            titleFontColor: "#333",
            titleFontFamily: "Microsoft Yahei",
            titleFontWeight: "bold",
            titlePaddingTop: 0,
            titlePaddingRight: 0,
            titlePaddingBottom: 6,
            titlePaddingLeft: 8,
            displayDesc: !0,
            descFontSize: 14,
            descFontColor: "#6b6b6b",
            descFontFamily: "Microsoft Yahei",
            paddingTop: 8,
            paddingRight: 16,
            paddingBottom: 8,
            paddingLeft: 16,
            backgroundColor: "#fff",
            hoverColor: "#ca0c16"
          }
        });
  }
  var adsMark = "user_define";
  /**
   * T位置的渲染
   * @param {渲染位置的domId} renderDomId
   * @param {渲染的位置，位置是T0,T3,T7,T38,T43,T48} pos
   */
  window.p4CSDNRender_T = function(renderDomId, pos) {
    var renderDom = document.getElementById(renderDomId);
    if (renderDom && pos) {
      p4CSDNTBootstrap(renderCallBack, renderDom, pos);
    }
  };
  /**
   * 侧边栏位置的渲染
   * @param {渲染位置的domId} renderDomId
   */
  window.p4CSDNRender_S = function(renderDomId) {
    var renderDom = document.getElementById(renderDomId);
    if (renderDom) {
      p4CSDNSBootstrap(renderCallBack, renderDom);
    }
  };
})(window, document);
