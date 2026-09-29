
[torabo-tsuki LP](https://github.com/sekigon-gonnoc/torabo-tsuki-lp)用のZMKファームウェア

* _centralがついているuf2をトラックボールがついている方に、_peripheralを反対側に書き込んでください
* キーマップはkeymap-editorおよびzmk-studioで編集できます

トラックパッドの操作は `snippets/input-split-listener/input-split-listener.overlay` で変更できます。通常レイヤーではタップが `Alt+←`、レイヤー1では縦スクロールが `Ctrl+=` / `Ctrl+-` によるズームでタップが `Ctrl+0`、レイヤー2では縦スクロールが音量調整でタップがミュートです。各 `bindings` の `&kp` を編集するとショートカットを変更できます。ズームはOSのピンチ操作ではなくキー操作です。通常スクロール速度は `&trackpad_responsive_scroll 1 4` の倍率で変更でき、最初の1目盛りはすぐに出ます。レイヤー1・2の感度はそれぞれ `&zip_scroll_scaler 1 3` で変更できます。一続きのスクロールで操作モードを固定し、レイヤーを途中で切り替えた場合は残りの入力を破棄します。レイヤー切り替え直後の入力は `transition-guard-ms`（現在100ms）でも抑制します。慣性は通常スクロールだけに適用し、強さは左側の `torabo_tsuki_lp_left.conf` の `CONFIG_IQS7211E_SCROLLER_INERTIA_DECAY_PERMILLE` で変更できます。この動作には左右両方のファームウェアの更新が必要です。

自動マウスレイヤー6のクリックキーは J/O/P（左/中/右クリック）です。配置と左親指の日本語切り替えキーは `config/keymap.keymap` で編集できます。
