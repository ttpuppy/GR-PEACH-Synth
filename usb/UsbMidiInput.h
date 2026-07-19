// UsbMidiInput.h
// USB0をUSB-MIDIデバイスとして使うためのラッパー。mbed-osの標準USBMIDIクラスを使用。
//
// 【APIについての注記 - 修正済み】
// mbed-os 6のUSBMIDI::attach()は、mbed classic時代のドキュメント（多くのcookbook
// 記事に見られる `void attach(void (*fptr)(MIDIMessage))`）とは異なり、
// `void attach(mbed::Callback<void()> callback)` という「引数なし」の通知コールバック
// に変更されている（公式リファレンス mbed-os v6.16 USBMIDI Class Reference で確認）。
// 実際のMIDIMessageは、通知を受けてから `bool read(MIDIMessage*)` で個別に
// 取り出す設計になっている。`readable()`で読み出し可能かどうかも確認できる。
// なお公式ドキュメントには「readable()/read()を機能させるにはattach()の呼び出しが
// 必須」と明記されている（コールバック自体が空でも呼ぶ必要がある）。
//
// 本実装では、USB割り込みコンテキストで呼ばれるattach()のコールバックは
// フラグを立てるだけにとどめ、実際のread()呼び出しと後続のディスパッチは
// poll()（メインループから周期的に呼ぶ）側で行う。
//
// 【ポータビリティについての注記】
// USB2.0デバイスコントローラのプロトコル・ディスクリプタ処理をゼロから
// 自前実装するのは現実的ではないため、USB周りに限りmbed-osのUSBDevice
// ライブラリ（USBMIDI/MIDIMessage）にあえて依存している。
// 将来FreeRTOSへ移行する際は、USBMIDIクラスの内部実装（mbed-os内の
// USBDevice/USBMIDI配下の.cpp/.h一式）をそのまま持ってくるか、
// TinyUSB等のRTOS非依存なUSBスタックに置き換えることを推奨する。
//
// 【既知の制約】
// USB0をUSB SerialとUSB MIDIの複合(コンポジット)デバイスとして同時に
// 見せるには、USBMIDIとUSBSerial(CDC)を1つのUSBDeviceに束ねるカスタム
// ディスクリプタの実装が必要（mbed-os標準では個別クラスとしてのみ提供）。
// 本ファイルはMIDI機能のみを実装しており、複合化は未着手（README参照）。

#pragma once
#include "mbed.h"
#include "USBMIDI.h"
#include <functional>
#include <cstdint>

namespace synth {

using UsbNoteOnCallback  = std::function<void(uint8_t channel, uint8_t note, uint8_t velocity)>;
using UsbNoteOffCallback = std::function<void(uint8_t channel, uint8_t note)>;
using UsbCcCallback      = std::function<void(uint8_t channel, uint8_t controller, uint8_t value)>;
using UsbProgramChangeCallback = std::function<void(uint8_t channel, uint8_t program)>;
using UsbPitchBendCallback = std::function<void(uint8_t channel, int16_t bend14bit)>;

class UsbMidiInput {
public:
    UsbMidiInput() {
        // readable()/read()を機能させるためにattach()の呼び出しが必須
        // （公式リファレンスに明記）。コールバック自体はフラグを立てるのみ。
        midi_.attach(callback(this, &UsbMidiInput::onMidiEventAvailable));
    }

    void onNoteOn(UsbNoteOnCallback cb) { noteOnCb_ = cb; }
    void onNoteOff(UsbNoteOffCallback cb) { noteOffCb_ = cb; }
    void onCc(UsbCcCallback cb) { ccCb_ = cb; }
    void onProgramChange(UsbProgramChangeCallback cb) { programChangeCb_ = cb; }
    void onPitchBend(UsbPitchBendCallback cb) { pitchBendCb_ = cb; }

    // メインループから周期的に呼び出す。保留中のUSB MIDIメッセージを
    // すべて読み出してディスパッチする。
    void poll() {
        MIDIMessage msg;
        while (midi_.readable() && midi_.read(&msg)) {
            dispatch(msg);
        }
    }

private:
    // USB割り込みコンテキストから呼ばれる。重い処理はしない。
    void onMidiEventAvailable() {
        // 現状はpoll()側でreadable()を都度確認しているため、フラグは不要。
        // 将来イベント駆動に変える場合に備えて残してある。
    }

    // 【修正】mbed-osのMIDIMessageのアクセサ(type()/velocity()/channel()/key()/
    // controller()/value()/program()/pitch())はconst宣言されていないため、
    // const参照では呼び出せない（ビルドエラー: "this argument...has type
    // 'const MIDIMessage', but function is not marked const"）。
    // 非const参照に変更して対応。
    void dispatch(MIDIMessage& msg) {
        switch (msg.type()) {
            case MIDIMessage::NoteOnType:
                if (msg.velocity() == 0) {
                    if (noteOffCb_) noteOffCb_(msg.channel(), msg.key());
                } else {
                    if (noteOnCb_) noteOnCb_(msg.channel(), msg.key(), msg.velocity());
                }
                break;
            case MIDIMessage::NoteOffType:
                if (noteOffCb_) noteOffCb_(msg.channel(), msg.key());
                break;
            case MIDIMessage::ControlChangeType:
                if (ccCb_) ccCb_(msg.channel(), msg.controller(), msg.value());
                break;
            case MIDIMessage::ProgramChangeType:
                if (programChangeCb_) programChangeCb_(msg.channel(), msg.program());
                break;
            case MIDIMessage::PitchWheelType:
                // MIDIMessage::pitch() が14bit値(0-16383, センター8192)を返す
                // （出典: mbed-os v6.16 MIDIMessage Class Reference "Read the pitch value"）
                if (pitchBendCb_) pitchBendCb_(msg.channel(), static_cast<int16_t>(msg.pitch()) - 8192);
                break;
            default:
                break;
        }
    }

    USBMIDI midi_;
    UsbNoteOnCallback noteOnCb_;
    UsbNoteOffCallback noteOffCb_;
    UsbCcCallback ccCb_;
    UsbProgramChangeCallback programChangeCb_;
    UsbPitchBendCallback pitchBendCb_;
};

} // namespace synth
