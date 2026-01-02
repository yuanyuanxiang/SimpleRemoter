
/*
鍘熸枃锛歨ttps://github.com/yuanyuanxiang/SimpleRemoter/releases/tag/v1.0.1.1

鑷獀1.1.1鐗堟湰寮€濮嬶紝涓绘帶绋嬪簭闇€瑕佹巿鏉冿紝骞朵笖浼氳嚜鍔ㄨ繛鎺ュ埌鎺堟潈鏈嶅姟鍣紝鎮ㄥ彲浠ヨ仈绯讳綔鑰呰姹傛巿鏉冦€?
濡傛灉瀵硅繖涓湁鎰忚锛岃浣跨敤鏃╂湡鐗堟湰锛?v1.0.8锛夈€傝嚜琛屼慨鏀瑰拰缂栬瘧绋嬪簭锛屼篃鍙互瑙ｅ喅璇ラ棶棰橈紙鍙傝€?#91锛夈€?

浣滆€呮姇鍏ヤ簡涓氫綑绮惧姏鏉ョ淮鎶ゃ€佹洿鏂版湰杞欢锛屽紑婧愪粎渚涘涔犱氦娴佷箣鐢紝鐩堝埄骞堕潪涓昏鐩殑銆?
鑻ラ渶浣跨敤鍙戝竷鐗堟湰锛岄』鑾峰緱鎺堟潈锛岄渶瑕佹敮浠樹竴瀹氱殑鎺堟潈璐圭敤銆?

浣犲彲浠ヨ嚜鐢变慨鏀逛唬鐮佸苟鑷缂栬瘧浣跨敤锛堣鍙傝€冧笂杩伴棶棰橈細#91锛夛紝姝ゆ儏鍐典笅涓嶆敹鍙栦换浣曡垂鐢ㄣ€?
寤鸿鐢ㄦ埛浼樺厛灏濊瘯鑷缂栬瘧锛屾垨娴嬭瘯鏃х増鏈槸鍚﹀凡鑳芥弧瓒抽渶姹傦紱濡備粛鏈夐渶瑕佷笖鍏峰棰勭畻锛屽彲鍐嶈€冭檻姝ｅ紡鎺堟潈銆?

濡傚凡鑾峰緱鎺堟潈锛屽悗缁彂甯冪殑鏂扮増鏈彲缁х画浣跨敤锛屼笖鏈娇鐢ㄥ畬鐨勬巿鏉冩椂闂村皢鑷姩椤哄欢鑷虫柊鐗堟湰銆?

鈿狅笍 鏈蒋浠朵粎闄愪簬鍚堟硶銆佹褰撱€佸悎瑙勭殑鐢ㄩ€斻€傜姝㈠皢鏈蒋浠剁敤浜庝换浣曡繚娉曘€佹伓鎰忋€佷镜鏉冩垨杩濆弽閬撳痉瑙勮寖鐨勮涓恒€?
浣滆€呬笉瀵逛换浣曞洜婊ョ敤杞欢鎵€寮曞彂鐨勬硶寰嬭矗浠汇€佹崯瀹虫垨浜夎鎵挎媴浠讳綍璐ｄ换锛屽苟淇濈暀鍦ㄥ彂鐜版垨鎬€鐤戜笉褰撶敤閫旀椂鎷掔粷鎴栫粓姝㈡巿鏉冪殑鏉冨埄銆?

--------------------------------------------------------------------------------------------------------------

Starting from this version, the main control program requires authorization and will automatically
connect to the authorization server. You may contact the author to request a license.
If you have concerns about this mechanism, please use an earlier version (prior to v1.0.8).
Alternatively, you may modify and compile the program yourself to bypass this requirement (see #91).

The author maintains and updates this software in their spare time. It is open-sourced solely for
educational and non-commercial use; profit is not the primary goal.
To use the official release version, a license must be obtained, which requires payment of a licensing fee.

You are free to modify the code and compile it for your own use (please refer to the note above: #91).
No fees are charged in this case.
Users are encouraged to first attempt self-compilation or test an earlier version to see if it meets their needs.
If further functionality is required and budget is available, you may then consider obtaining a formal license.

If a license is obtained, future versions of the software can continue to be used under the same license,
and any remaining license time will be automatically carried over to the new version.

鈿狅笍 This software is intended for lawful, legitimate, and compliant use only.
Any use of this software for illegal, malicious, infringing, or unethical purposes is strictly prohibited.
The author shall not be held liable for any legal issues, damages, or disputes resulting from misuse of
the software, and reserves the right to refuse or revoke authorization if improper use is discovered or suspected.
*/

// 涓绘帶绋嬪簭鍞竴鏍囪瘑
// 鎻愮ず: 淇敼杩欎釜鍝堝笇鍙兘閫犳垚涓€浜涘姛鑳藉彈闄愶紝鑷富鎺х殑v1.1.1鐗堟湰璧凤紝绋嬪簭鐨勮澶氬姛鑳戒緷璧栦簬璇ュ搱甯?
// 鍥犳锛屽浜庢兂鐮撮櫎绋嬪簭鎺堟潈闄愬埗鐨勮涓猴紝寤鸿鍩轰簬v1.1.1鐗堟湰锛岀敋鑷充娇鐢ㄦ棤闇€鎺堟潈鐨勭増鏈紙濡傝兘婊¤冻闇€姹傦級.
// 褰撶劧杩欎簺鏃╂湡鐗堟湰娌℃湁鍖呭惈闂淇鍜屾柊鐨勫姛鑳?
#define MASTER_HASH "61f04dd637a74ee34493fc1025de2c131022536da751c29e3ff4e9024d8eec43"
