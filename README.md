# DST Sync

Công cụ Windows (1 file .exe, có giao diện) để bạn bè trao đổi save world **Don't Starve Together** qua một repo GitHub.

- **Pull**: lấy world mới nhất từ repo về máy, chiếm "khóa host".
- **Push**: đẩy world lên repo, nhả khóa.
- **Phát hiện & xử lý xung đột**, **tự backup**, **màn hình Cài đặt**, **đi kèm PortableGit**.

## 1. Chuẩn bị repo (một người làm, một lần)

1. Tạo repo **private** trên GitHub, tick *Add a README file* (để repo có sẵn nhánh `main`).
2. Settings > Collaborators: mời bạn bè.
3. Mỗi người tạo **Fine-grained token** (GitHub > Settings > Developer settings > Personal access tokens), chọn đúng repo này, quyền **Contents: Read and write**.

## 2. Biên dịch (người làm tool, một lần)

Cần MinGW-w64 (`g++`) **hoặc** Visual Studio (`cl`) trong PATH.

```
build.bat          -> release\dstsync.exe
make-release.bat   -> build + tải PortableGit vào release\git + nén dstsync-release.zip
```

Gửi `dstsync-release.zip` cho bạn bè, giải nén là dùng, không cần cài gì (đã gồm Git).
Nếu gửi bản không kèm Git, lần đầu mở app sẽ hỏi tự tải PortableGit (nút **Tải Git** làm cùng việc đó).

## 3. Dùng

Lần đầu mở sẽ hiện **Cài đặt**:

| Ô | Ý nghĩa |
|---|---|
| Tên hiển thị | Tên bạn, dùng để ghi chủ khóa host. Mỗi người một tên khác nhau |
| URL repo | `https://github.com/ten/repo.git` |
| Token | Token ở bước 1 (để trống nếu máy đã đăng nhập Git sẵn) |
| Thư mục repo | Nơi tool giữ bản sao repo. Tool tự quản lý, đừng sửa tay |
| Thư mục save | Mặc định `Documents\Klei\DoNotStarveTogether` (đã xử lý OneDrive) |
| Tên world | Thư mục cluster, ví dụ `Cluster_1` (bấm **Quét** để liệt kê) |
| Thư mục backup / Số bản giữ | Nơi lưu và số backup giữ lại (mặc định 5) |

**Quy trình chơi**

1. Muốn host: **Pull** (kiểm tra không ai đang giữ khóa) rồi mở game/server.
2. Chơi xong: **tắt hẳn game/server**, bấm **Push**.

Tool từ chối Pull/Push khi phát hiện tiến trình `dontstarve_*` đang chạy, tránh ghi vào save đang mở.

## 4. Cách xử lý xung đột và backup

- **Khóa host** (`lock_<world>.txt` trong repo): ai Pull sẽ giữ khóa, người khác Pull sẽ bị cảnh báo.
- Pull chiếm khóa **trước** rồi mới đụng vào save local. Nếu chiếm khóa thất bại, save local không đổi.
- Push phát hiện xung đột khi: (a) người khác đang giữ khóa, hoặc (b) world trên repo đã đổi kể từ lần Pull cuối của bạn. Khi đó có 3 lựa chọn:
  - **Có**: ghi đè bản repo bằng bản của bạn (bản repo cũ được backup về máy trước).
  - **Không**: đẩy bản của bạn vào thư mục riêng `Cluster_1_conflict_<tên>_<giờ>` trên repo, không ghi đè gì.
  - **Hủy**: không làm gì.
- **Backup tự động** (thư mục `backups\`): save local trước mỗi Pull, và bản repo trước mỗi lần ghi đè. Tool chỉ giữ N bản mới nhất.
- Bản sao repo trên máy luôn được đưa về đúng như remote (`fetch` + `reset --hard`), nên không bao giờ có xung đột merge của git.

## 5. Bảo mật & giới hạn

- `cluster_token.txt` (token server Klei) và `*.log` **không bao giờ** được đẩy lên repo. Khi Pull, token riêng của máy bạn được giữ nguyên.
- Token GitHub được lưu **dạng chữ thường** trong `dstsync.cfg` và trong cấu hình git của thư mục repo. Dùng fine-grained token chỉ cho repo này, và đừng gửi file `dstsync.cfg` cho ai.
- Khóa là "khóa lịch sự": hai người bấm Pull cùng lúc thì một người sẽ bị từ chối ở bước push khóa, nhưng nếu ai đó cố ý ghi đè thì tool không ngăn được (chỉ cảnh báo).
- Git lưu mọi phiên bản nên repo sẽ lớn dần. Mỗi file phải dưới 100MB (tool chặn ở 95MB). Khi repo quá nặng, hãy tạo repo mới hoặc xóa lịch sử.
- Chỉ đồng bộ thư mục cluster. Mod vẫn mỗi người tự tải (danh sách mod nằm trong `modoverrides.lua` của cluster).
- Tool này **chưa được kiểm thử trên máy Windows thật** vào lúc viết. Hãy thử với world nhỏ trước.

## Cấu trúc dự án

```
src/main.cpp        toàn bộ chương trình
res/app.rc|manifest giao diện hiện đại (Common Controls 6)
build.bat           biên dịch (MinGW hoặc MSVC)
get-git.ps1/.bat    tải PortableGit vào thư mục git\
make-release.bat    build + đóng gói zip cho bạn bè
```
