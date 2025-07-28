
/*
原文：https://github.com/yuanyuanxiang/SimpleRemoter/releases/tag/v1.0.1.1

自v1.1.1版本开始，主控程序需要授权，并且会自动连接到授权服务器，您可以联系作者请求授权。
如果对这个有意见，请使用早期版本（<v1.0.8）。自行修改和编译程序，也可以解决该问题（参考 #91）。

作者投入了业余精力来维护、更新本软件，开源仅供学习交流之用，盈利并非主要目的。
若需使用发布版本，须获得授权，需要支付一定的授权费用。

你可以自由修改代码并自行编译使用（请参考上述问题：#91），此情况下不收取任何费用。
建议用户优先尝试自行编译，或测试旧版本是否已能满足需求；如仍有需要且具备预算，可再考虑正式授权。

如已获得授权，后续发布的新版本可继续使用，且未使用完的授权时间将自动顺延至新版本。

⚠️ 本软件仅限于合法、正当、合规的用途。禁止将本软件用于任何违法、恶意、侵权或违反道德规范的行为。
作者不对任何因滥用软件所引发的法律责任、损害或争议承担任何责任，并保留在发现或怀疑不当用途时拒绝或终止授权的权利。

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

⚠️ This software is intended for lawful, legitimate, and compliant use only.
Any use of this software for illegal, malicious, infringing, or unethical purposes is strictly prohibited.
The author shall not be held liable for any legal issues, damages, or disputes resulting from misuse of 
the software, and reserves the right to refuse or revoke authorization if improper use is discovered or suspected.
*/

// 主控程序唯一标识
// 提示: 修改这个哈希可能造成一些功能受限，自主控的v1.1.1版本起，程序的诸多功能依赖于该哈希.
// 因此，对于想破除程序授权限制的行为，建议基于v1.1.1版本，甚至使用无需授权的版本（如能满足需求）.
// 当然这些早期版本没有包含问题修复和新的功能.
#define MASTER_HASH "61f04dd637a74ee34493fc1025de2c131022536da751c29e3ff4e9024d8eec43"
