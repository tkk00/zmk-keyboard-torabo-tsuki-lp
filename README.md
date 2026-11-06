
[torabo-tsuki LP](https://github.com/sekigon-gonnoc/torabo-tsuki-lp)用のZMKファームウェア

* _centralがついているuf2をトラックボールがついている方に、_peripheralを反対側に書き込んでください
* キーマップはkeymap-editorおよびzmk-studioで編集できます

トラックパッドの操作は `snippets/input-split-listener/input-split-listener.overlay` で変更できます。通常レイヤーではタップは無効、レイヤー1では縦スクロールが `Ctrl+=` / `Ctrl+-` によるズームでタップが `Ctrl+0`、レイヤー2では縦スクロールが音量調整でタップがミュートです。音量の増減方向は通常スクロールと逆です。各 `bindings` の `&kp` を編集するとショートカットを変更できます。音量の方向を変える場合は `trackpad_volume_scroll` の2つの `bindings` を入れ替えます。ズームはOSのピンチ操作ではなくキー操作です。通常スクロール速度は `&trackpad_responsive_scroll 1 5` の倍率で変更でき、最初の1目盛りはすぐに出ます。レイヤー1・2の感度はそれぞれ `&zip_scroll_scaler 1 3` で変更できます。レイヤー1・2のキー変換後は `zoom_consumed` / `volume_consumed` が元のスクロール量とタップ通知を無効にします。インターセプトプロセッサーは150ms以内に続くスクロールを一続きとみなし、途中でレイヤーが変わった場合は別の動作へ切り替わる入力を破棄します。ドライバーは元のままなので指の接触単位の判定はできず、シングルタップには元の約400msの待ち時間があります。レイヤー1・2キーは押した時点でレイヤーを有効にし、短いタップでは従来の文字を送ります。慣性は左側の `torabo_tsuki_lp_left.conf` の `CONFIG_IQS7211E_SCROLLER_INERTIA_DECAY_PERMILLE` で調整できます。レイヤー1・2でも慣性の残りがキー操作として送られます。この動作には左右両方のファームウェアの更新が必要です。

自動マウスレイヤー6のクリックキーは J（左）、O（中）、P（右）です。基本レイヤーで K を長押しするとスクロールレイヤー5に入り、J / L で `Alt+←` / `Alt+→` を入力できます。レイヤー0・1の J は長押しで左 Win、レイヤー2の J は左矢印です。
