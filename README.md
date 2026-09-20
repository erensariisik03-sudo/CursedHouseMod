# CursedHouseMod — exact chat-targeted limit patch

Bu sürüm `dump_dosyasi.cs` içindeki gerçek oyun `chat` sınıfını hedefler.

Dump'ta:
- `public class chat : MonoBehaviourPunCallbacks // TypeDefIndex: 3387`
- `TMP_InputField inputField; // 0x18`
- `chat.OnEnable() = 0xF696A0`
- `chat.sendMessage() = 0xF69758`
- `chat.Update() = 0xF6A6CC`

Kullanılan TMP adresleri:
- `TMP_InputField.set_characterLimit = 0x34AA4D0`
- `TMP_InputField.SetText = 0x34A92B0`
- `TMP_InputField.SetTextWithoutNotify = 0x34A9440`
- `TMP_InputField.Append(string) = 0x34B47DC`
- `TMP_InputField.Append(char) = 0x34B4884`
- `TMP_InputField.Insert(char) = 0x34B4CF8`
- `TMP_InputField.ActivateInputFieldInternal = 0x34AE794`
- `TMP_InputField.UpdateTouchKeyboardFromEditChanges = 0x34B1A68`
- `TMP_InputField.m_CharacterLimit = 0x114`

Bu sürüm özellikle `chat.inputField` nesnesini `chat + 0x18` üzerinden bulur ve limitini:
`0`
yapar.

Önemli: Bu test sürümü Android klavyeyi değiştirmez. Önce gerçekten oyunun chat alanındaki limit kaynağını izole eder. Böylece logda hedefin gerçekten vurulup vurulmadığını görebiliriz.

Log:
```bash
adb logcat -c
adb logcat -s CursedHouseChat:V
```

Beklenen:
```text
chat.OnEnable this=...
chat.OnEnable AFTER: chat=... inputField=... characterLimit=...
chat.Update: ... characterLimit X -> 0
TMP_InputField.Append...
TMP_InputField.Insert...
```

Çıktı:
`build/libcursedhouse_chat.so`

ARM32 adresleme:
`loadBias + RVA + Thumb(+1)`.
`-0x10000` kullanılmaz.
