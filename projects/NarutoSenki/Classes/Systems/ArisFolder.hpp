#pragma once
#include "Utils/Cocos2dxHelper.hpp"

#include <cstdio>
#include <string>
#include <vector>

#if (CC_TARGET_PLATFORM == CC_PLATFORM_ANDROID)
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

/**
 * ArisFolder
 *
 * Folder data di penyimpanan HP: /sdcard/Aris/  (hanya Android).
 * Isinya meniru folder Resources: Unit/, UI/, Audio/, Select/, lua/, dst. File di sini MENIMPA file
 * dengan nama sama di dalam APK, jadi XML, plist, gambar, dan konfigurasi bisa diubah tanpa build ulang.
 *
 *   /sdcard/Aris/Characters.ini   konfigurasi AI (alias, summon, buff, catch, ...)
 *   /sdcard/Aris/Tutorial.ini     panduan Characters.ini (hanya dokumentasi)
 *   /sdcard/Aris/Unit/...         menimpa Resources/Unit/...
 *   /sdcard/Aris/lua/...          skrip Lua (main.lua, ui/, core/, ...) disalin dari APK dan bisa diedit langsung;
 *                                 menimpa lua/ di APK. Hapus file/folder untuk kembali ke bawaan.
 *
 * prepare() aman dipanggil berulang. Jika izin penyimpanan belum diberikan, folder belum bisa dibuat
 * dan game memakai isi APK; setelah izin diberikan dan game dibuka ulang, folder dipakai.
 */
class ArisFolder
{
public:
	static constexpr const char *kConfigName = "Characters.ini";
	static constexpr const char *kTutorialName = "Tutorial.ini";
	// Salinan bawaan di APK (lua/Aichar/...)
	static constexpr const char *kBundledConfig = "Aichar/Characters.ini";
	static constexpr const char *kBundledTutorial = "Aichar/Tutorial.ini";

	// Skrip Lua bawaan di APK yang disalin ke /sdcard/Aris/lua/ (tanpa Aichar/ dan window.lua).
	static const std::vector<std::string> &luaFiles()
	{
		static const std::vector<std::string> files = {
			"class/basic.lua", "config.lua", "core/audio.lua", "core/init.lua", "core/mode.lua",
			"framework/audio.lua", "framework/cocos2dx.lua", "framework/cocos2dx/Event.lua",
			"framework/cocos2dx/Geometry.lua", "framework/cocos2dx/Global.lua", "framework/cocos2dx/LayerEx.lua",
			"framework/cocos2dx/NodeEx.lua", "framework/cocos2dx/ObjectBinding.lua", "framework/cocos2dx/OpenGL.lua",
			"framework/cocos2dx/SceneEx.lua", "framework/cocos2dx/SpriteEx.lua", "framework/debug.lua",
			"framework/deprecated.lua", "framework/display.lua", "framework/filter.lua", "framework/functions.lua",
			"framework/init.lua", "framework/json.lua", "framework/scheduler.lua", "framework/shortcodes.lua",
			"framework/toluaEx.lua", "framework/transition.lua", "framework/ui.lua", "main.lua",
			"ui/CreditLayer.lua", "ui/GameScene.lua", "ui/SelectButton.lua", "ui/SelectLayer.lua", "ui/SkillLayer.lua",
			"ui/StartMenu.lua", "ui/init.lua", "utils/event.lua", "utils/hook.lua", "utils/math.lua", "utils/save.lua",
			"utils/utils.lua"};
		return files;
	}

	static std::string root()
	{
#if (CC_TARGET_PLATFORM == CC_PLATFORM_ANDROID)
		return "/sdcard/Aris/";
#else
		return std::string();
#endif
	}

	static bool isReady() { return ready(); }

	// Buat folder, pasang search path (diprioritaskan di atas APK), salin file bawaan jika belum ada.
	static bool prepare()
	{
#if (CC_TARGET_PLATFORM == CC_PLATFORM_ANDROID)
		if (ready())
			return true;

		const std::string r = root();
		struct stat st;
		if (stat(r.c_str(), &st) != 0 && mkdir(r.c_str(), 0777) != 0)
		{
			static bool logged = false;
			if (!logged)
			{
				logged = true;
				CCLOG("[Aris] %s tidak bisa dibuat (izin penyimpanan belum diberikan?), memakai isi APK", r.c_str());
			}
			return false;
		}
		if (stat(r.c_str(), &st) != 0 || !S_ISDIR(st.st_mode))
			return false;

		// Sidik jari skrip Lua di APK dihitung SEBELUM search path Aris dipasang. Setelah dipasang, membaca
		// "lua/main.lua" akan mengembalikan salinan di Aris (yang sudah Anda edit), bukan yang ada di APK.
		const std::string luaHash = bundledLuaHash();

		// Search path: Aris/ dan Aris/lua/ lebih dulu daripada isi APK.
		auto fileUtils = FileUtils::sharedFileUtils();
		std::vector<std::string> paths = fileUtils->getSearchPaths();
		const std::string add[] = {r, r + "lua/"};
		for (int i = 1; i >= 0; --i)
		{
			bool exists = false;
			for (auto &p : paths)
				if (p == add[i])
					exists = true;
			if (!exists)
				paths.insert(paths.begin(), add[i]);
		}
		fileUtils->setSearchPaths(paths);

		// Kerangka folder meniru Resources (kosong, hanya foldernya). lua/ dibuat oleh syncLua (berisi skrip).
		static const char *kFolders[] = {
			"Config", "Effects", "Fonts", "GameMode", "Maps", "UI", "UI/hpBar", "Select", "Result", "Report", "Record",
			"Ougis", "Unit", "Unit/Flog", "Unit/Guardian", "Unit/Kuchiyose", "Unit/Kugutsu", "Unit/Mon", "Unit/Ninja",
			"Unit/Projectile", "Unit/Tower"};
		for (auto dir : kFolders)
		{
			const std::string full = r + dir;
			if (stat(full.c_str(), &st) != 0)
				mkdir(full.c_str(), 0777);
		}

		syncLua(r, luaHash);

		// Karakter.ini hanya disalin jika belum ada (agar hasil edit tidak tertimpa);
		// Tutorial.ini selalu disegarkan karena hanya dokumentasi.
		copyBundled(kBundledConfig, r + kConfigName, false);
		copyBundled(kBundledTutorial, r + kTutorialName, true);

		// PNG lepas di UI/ Select/ Result/ Report/ Record/ Ougis/ dibaca langsung sebagai frame (tanpa atlas).
		SpriteFrameCache::sharedSpriteFrameCache()->setLooseRoot(r.c_str());

		ready() = true;
		CCLOG("[Aris] folder data aktif: %s", r.c_str());
		return true;
#else
		return false;
#endif
	}

	// Isi file teks dari path absolut. false jika tidak ada / tidak bisa dibaca.
	static bool readFile(const std::string &path, std::string &out)
	{
		FILE *fp = fopen(path.c_str(), "rb");
		if (!fp)
			return false;
		fseek(fp, 0, SEEK_END);
		long size = ftell(fp);
		fseek(fp, 0, SEEK_SET);
		if (size < 0)
		{
			fclose(fp);
			return false;
		}
		out.resize((size_t)size);
		size_t n = size > 0 ? fread(&out[0], 1, (size_t)size, fp) : 0;
		fclose(fp);
		out.resize(n);
		return true;
	}

private:
	static void makeDirs(const std::string &path)
	{
		// buat tiap tingkat folder (mkdir -p)
		for (size_t i = 1; i < path.size(); ++i)
		{
			if (path[i] == '/')
			{
				struct stat st;
				const std::string part = path.substr(0, i);
				if (stat(part.c_str(), &st) != 0)
					mkdir(part.c_str(), 0777);
			}
		}
		struct stat st;
		if (stat(path.c_str(), &st) != 0)
			mkdir(path.c_str(), 0777);
	}

	static void removeTree(const std::string &path)
	{
		DIR *dir = opendir(path.c_str());
		if (!dir)
			return;
		while (struct dirent *e = readdir(dir))
		{
			const std::string name = e->d_name;
			if (name == "." || name == "..")
				continue;
			const std::string full = path + "/" + name;
			struct stat st;
			if (stat(full.c_str(), &st) == 0 && S_ISDIR(st.st_mode))
				removeTree(full);
			else
				unlink(full.c_str());
		}
		closedir(dir);
		rmdir(path.c_str());
	}

	// Sidik jari skrip Lua bawaan APK (nama + isi). Berubah hanya jika skrip di APK berubah.
	static std::string bundledLuaHash()
	{
		unsigned long long h = 1469598103934665603ULL; // FNV-1a
		for (auto &f : luaFiles())
		{
			unsigned long size = 0;
			unsigned char *data = FileUtils::sharedFileUtils()->getFileData(("lua/" + f).c_str(), "rb", &size);
			for (char c : f)
				h = (h ^ (unsigned char)c) * 1099511628211ULL;
			if (data)
			{
				for (unsigned long i = 0; i < size; ++i)
					h = (h ^ data[i]) * 1099511628211ULL;
				delete[] data;
			}
		}
		char buf[32];
		snprintf(buf, sizeof(buf), "%016llx", h);
		return buf;
	}

	// /sdcard/Aris/lua: salin skrip bawaan yang belum ada (hasil edit tidak tertimpa). Jika skrip di APK
	// berubah (APK baru), folder lama dipindah ke lua_old/ lalu skrip bawaan terbaru disalin ulang.
	static void syncLua(const std::string &r, const std::string &hash)
	{
		const std::string luaDir = r + "lua";
		const std::string marker = luaDir + "/.build";

		std::string old;
		if (readFile(marker, old) && old != hash)
		{
			removeTree(r + "lua_old");
			rename(luaDir.c_str(), (r + "lua_old").c_str());
			CCLOG("[Aris] skrip Lua di APK berubah, lua lama dipindah ke lua_old/ dan disalin ulang");
		}
		makeDirs(luaDir);

		int copied = 0;
		for (auto &f : luaFiles())
		{
			const std::string dest = luaDir + "/" + f;
			FILE *exists = fopen(dest.c_str(), "rb");
			if (exists)
			{
				fclose(exists);
				continue;
			}
			const size_t slash = dest.rfind('/');
			if (slash != std::string::npos)
				makeDirs(dest.substr(0, slash));
			copyBundled(("lua/" + f).c_str(), dest, true);
			++copied;
		}
		if (copied > 0)
			CCLOG("[Aris] %d skrip Lua disalin ke %s", copied, luaDir.c_str());

		FILE *fp = fopen(marker.c_str(), "wb");
		if (fp)
		{
			fwrite(hash.data(), 1, hash.size(), fp);
			fclose(fp);
		}
	}

	static bool &ready()
	{
		static bool value = false;
		return value;
	}

	static void copyBundled(const char *bundledPath, const std::string &dest, bool overwrite)
	{
		if (!overwrite)
		{
			FILE *exists = fopen(dest.c_str(), "rb");
			if (exists)
			{
				fclose(exists);
				return;
			}
		}
		unsigned long size = 0;
		unsigned char *data = FileUtils::sharedFileUtils()->getFileData(bundledPath, "rb", &size);
		if (!data)
		{
			CCLOG("[Aris] file bawaan %s tidak ditemukan di APK", bundledPath);
			return;
		}
		FILE *fp = fopen(dest.c_str(), "wb");
		if (fp)
		{
			fwrite(data, 1, size, fp);
			fclose(fp);
			CCLOG("[Aris] disalin: %s", dest.c_str());
		}
		else
			CCLOG("[Aris] gagal menulis %s", dest.c_str());
		delete[] data;
	}
};
