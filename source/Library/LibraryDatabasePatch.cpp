// Core
#include <Core/Log.h>

// rePlayer
#include <Library/LibraryDatabase.h>

// libarchive
#include <libarchive/archive.h>
#include <libarchive/archive_entry.h>

// zlib
#include <zlib.h>

namespace rePlayer
{
    void LibraryDatabase::Patch()
    {
        static constexpr uint32_t kSongsVersionWithArchive = 2;
        static constexpr eExtension kExtension_mdxPk = eExtension::_rk;
        static constexpr eExtension kExtension_qsfPk = eExtension::_rar;
        static constexpr eExtension kExtension_gsfPk = eExtension::_7z;
        static constexpr eExtension kExtension_2sfPk = eExtension::_gz;
        static constexpr eExtension kExtension_ssfPk = eExtension::_liq;
        static constexpr eExtension kExtension_dsfPk = eExtension::_rtm;
        static constexpr eExtension kExtension_psfPk = eExtension::_mgt;
        static constexpr eExtension kExtension_psf2Pk = eExtension::_fnk;
        static constexpr eExtension kExtension_usfPk = eExtension::_arch;
        static constexpr eExtension kExtension_snsfPk = eExtension::_coco;
        static constexpr eExtension kExtension_mbmPk = eExtension::_cba;
        static constexpr eExtension kExtension_musPk = eExtension::_a2t;
        static constexpr eExtension kExtension_eupPk = eExtension::_etx;
        if (SongsVersion() < kSongsVersionWithArchive)
        {
            for (auto* song : Songs())
            {
                struct ArchiveBuffer : public Array<uint8_t>
                {
                    static int ArchiveOpen(struct archive*, void*)
                    {
                        return ARCHIVE_OK;
                    }

                    static int ArchiveClose(struct archive*, void*)
                    {
                        return ARCHIVE_OK;
                    }

                    static int ArchiveFree(struct archive*, void*)
                    {
                        return ARCHIVE_OK;
                    }

                    static la_ssize_t ArchiveWrite(struct archive*, void* _client_data, const void* _buffer, size_t _length)
                    {
                        reinterpret_cast<ArchiveBuffer*>(_client_data)->Add(reinterpret_cast<const uint8_t*>(_buffer), uint32_t(_length));
                        return _length;
                    }
                } archiveBuffer;

                switch (auto ext = song->GetType().ext)
                {
                case kExtension_mdxPk:
                case kExtension_qsfPk:
                case kExtension_gsfPk:
                case kExtension_2sfPk:
                case kExtension_ssfPk:
                case kExtension_dsfPk:
                case kExtension_psfPk:
                case kExtension_psf2Pk:
                case kExtension_usfPk:
                case kExtension_snsfPk:
                {
                    auto oldFilename = GetFullpath(song);
                    auto songSheet = song->Edit();
                    songSheet->type.ext = ext == kExtension_mdxPk ? eExtension::_mdx
                        : ext == kExtension_qsfPk ? eExtension::_miniqsf
                        : ext == kExtension_gsfPk ? eExtension::_minigsf
                        : ext == kExtension_2sfPk ? eExtension::_mini2sf
                        : ext == kExtension_ssfPk ? eExtension::_minissf
                        : ext == kExtension_dsfPk ? eExtension::_minidsf
                        : ext == kExtension_psfPk ? eExtension::_minipsf
                        : ext == kExtension_psf2Pk ? eExtension::_minipsf2
                        : ext == kExtension_usfPk ? eExtension::_miniusf
                        : eExtension::_minisnsf;
                    songSheet->subsongs[0].isPackage = false;
                    songSheet->subsongs[0].isArchive = true;
                    Move(oldFilename, song, "Patch");
                }
                break;
                case kExtension_mbmPk:
                case kExtension_musPk:
                case kExtension_eupPk:
                {
                    auto oldFilename = GetFullpath(song);
                    auto songSheet = song->Edit();
                    songSheet->type.ext = ext == kExtension_mbmPk ? eExtension::_mbm
                        : ext == kExtension_musPk ? eExtension::_mus
                        : eExtension::_eup;
                    songSheet->subsongs[0].isPackage = false;
                    songSheet->subsongs[0].isArchive = true;
                    auto newFilename = GetFullpath(song);
                    auto oldFile = io::File::OpenForRead(oldFilename.c_str());
                    if (oldFile.IsValid())
                    {
                        auto newFile = io::File::OpenForWrite(newFilename.c_str());
                        if (newFile.IsValid())
                        {
                            oldFile.Seek(8);
                            Array<uint8_t> data(uint32_t(oldFile.GetSize() - 8));
                            oldFile.Read(data.Items(), data.Size());
                            newFile.Write(data.Items(), data.Size());
                        }
                        oldFile = {};
                        if (!io::File::Delete(oldFilename.c_str()))
                            Log::Warning("Can't delete file \"%s\"\n", oldFilename.c_str());
                    }
                    break;
                }
                case eExtension::_cust:
                case eExtension::_smus:
                {
                    auto oldFilename = GetFullpath(song);
                    auto file = io::File::OpenForRead(oldFilename.c_str());
                    if (file.IsValid())
                    {
                        char buf[8];
                        file.Read(buf, 8);
                        if (memcmp(buf, ext == eExtension::_cust ? "CUST-PKG" : "SMUS-PKG", 8) == 0)
                        {
                            Array<uint8_t> data(uint32_t(file.GetSize() - 8));
                            file.Read(data.Items(), data.NumItems());
                            file = {};
                            if (!io::File::Delete(oldFilename.c_str()))
                                Log::Warning("Can't delete file \"%s\"\n", oldFilename.c_str());

                            auto songSheet = song->Edit();
                            songSheet->subsongs[0].isPackage = false;
                            songSheet->subsongs[0].isArchive = true;

                            file = io::File::OpenForWrite(GetFullpath(song).c_str());
                            if (file.IsValid())
                            {
                                data.Copy(buf, 8);
                                file.Write(data.Items(), data.NumItems());

                                songSheet->fileSize = data.Size<uint32_t>();
                                songSheet->fileCrc = crc32(0L, Z_NULL, 0);
                                songSheet->fileCrc = crc32_z(songSheet->fileCrc, data.Items(), data.Size<size_t>());
                            }
                        }
                    }
                    break;
                }
                case eExtension::_tfm:
                case eExtension::_rjp:
                case eExtension::_adsc:
                case eExtension::_tpu:
                case eExtension::_dns:
                case eExtension::_dum:
                case eExtension::_jpn:
                case eExtension::_kh:
                case eExtension::_mfp:
                case eExtension::_mco:
                case eExtension::_mcr:
                case eExtension::_pap:
                case eExtension::_pn:
                case eExtension::_qts:
                case eExtension::_sjs:
                case eExtension::_sdr:
                case eExtension::_osp:
                case eExtension::_thm:
                case eExtension::_dat:
                case eExtension::_mdst:
                case eExtension::_mod:
                case eExtension::_uds:
                case eExtension::_mus:
                case eExtension::_ksm:
                case eExtension::_sng:
                case eExtension::_sci:
                case eExtension::_rol:
                case eExtension::_kdm:
                {
                    auto oldFilename = GetFullpath(song);
                    auto file = io::File::OpenForRead(oldFilename.c_str());
                    if (file.IsValid())
                    {
                        char buf[8];
                        file.Read(buf, 8);
                        if (memcmp(buf, "TFMX-MOD", 8) == 0 || memcmp(buf, "RiJo-MOD", 8) == 0 || memcmp(buf, "ADSC-MOD", 8) == 0
                            || memcmp(buf, "DIRK-MOD", 8) == 0 || memcmp(buf, "DNSY-MOD", 8) == 0 || memcmp(buf, "IFGM-MOD", 8) == 0
                            || memcmp(buf, "JSPG-MOD", 8) == 0 || memcmp(buf, "KSHD-MOD", 8) == 0 || memcmp(buf, "MFPK-MOD", 8) == 0
                            || memcmp(buf, "MCOD-MOD", 8) == 0 || memcmp(buf, "PAPK-MOD", 8) == 0 || memcmp(buf, "PKNS-MOD", 8) == 0
                            || memcmp(buf, "QTST-MOD", 8) == 0 || memcmp(buf, "SDPR-MOD", 8) == 0 || memcmp(buf, "SHDM-MOD", 8) == 0
                            || memcmp(buf, "SHPK-MOD", 8) == 0 || memcmp(buf, "TSHN-MOD", 8) == 0 || memcmp(buf, "PLRM-MOD", 8) == 0
                            || memcmp(buf, "MDST-MOD", 8) == 0 || memcmp(buf, "STAM-MOD", 8) == 0 || memcmp(buf, "UNDV-MOD", 8) == 0
                            || memcmp(buf, "STER-SID", 8) == 0 || memcmp(buf, "KENS-ADB", 8) == 0 || memcmp(buf, "ABTR-ADB", 8) == 0
                            || memcmp(buf, "SIRA-ADB", 8) == 0 || memcmp(buf, "VICO-ADB", 8) == 0 || memcmp(buf, "KENS-KDM", 8) == 0)
                        {
                            auto smplOffset = file.Read<uint32_t>();
                            auto mdatSize = smplOffset - (ext == eExtension::_tfm ? 20 : 12);
                            auto smplSize = uint32_t(file.GetSize() - smplOffset);
                            file.Seek(ext == eExtension::_tfm ? 20 : 12);

                            auto songSheet = song->Edit();
                            if (ext == eExtension::_tfm)
                                songSheet->type.ext = eExtension::_mdat;
                            else if (ext == eExtension::_qts)
                                songSheet->type.ext = eExtension::_4v;
                            songSheet->subsongs[0].isPackage = true;
                            songSheet->subsongs[0].isArchive = true;

                            auto* archive = archive_write_new();
                            archive_write_set_format_7zip(archive);
                            archive_write_open2(archive, &archiveBuffer, archiveBuffer.ArchiveOpen, ArchiveBuffer::ArchiveWrite, archiveBuffer.ArchiveClose, archiveBuffer.ArchiveFree);
                            auto entry = archive_entry_new();
                            std::string title = songSheet->name.String();
                            io::File::CleanFilename(title.data());
                            if (ext == eExtension::_tfm)
                                title = "mdat." + title;
                            else if (ext == eExtension::_rjp)
                                title += ".sng";
                            else if (ext == eExtension::_adsc)
                                title += ".adsc";
                            else if (ext == eExtension::_tpu)
                                title = "tpu." + title;
                            else if (ext == eExtension::_dns)
                                title = "dns." + title;
                            else if (ext == eExtension::_dum)
                                title += ".dum";
                            else if (ext == eExtension::_jpn)
                                title = "jpn." + title;
                            else if (ext == eExtension::_kh)
                                title += ".kh";
                            else if (ext == eExtension::_mfp)
                                title = "mfp." + title;
                            else if (ext == eExtension::_mco)
                                title = "mco." + title;
                            else if (ext == eExtension::_mcr)
                                title = "mcr." + title;
                            else if (ext == eExtension::_pap)
                                title += ".pap";
                            else if (ext == eExtension::_pn)
                                title = "pn." + title;
                            else if (ext == eExtension::_qts)
                                title += ".4v";
                            else if (ext == eExtension::_sjs)
                                title = "sjs." + title;
                            else if (ext == eExtension::_sdr)
                                title = "sdr." + title;
                            else if (ext == eExtension::_osp)
                                title += ".osp";
                            else if (ext == eExtension::_thm)
                                title = "thm." + title;
                            else if (ext == eExtension::_dat)
                                title += ".dat";
                            else if (ext == eExtension::_mdst)
                                title = "mdst." + title;
                            else if (ext == eExtension::_mod)
                                title += ".mod";
                            else if (ext == eExtension::_uds)
                                title = "uds." + title;
                            else if (ext == eExtension::_mus)
                                title += ".mus";
                            else if (ext == eExtension::_ksm)
                                title += ".ksm";
                            else if (ext == eExtension::_sng)
                                title += ".sng";
                            else if (ext == eExtension::_sci)
                                title += ".sci";
                            else if (ext == eExtension::_rol)
                                title += ".rol";
                            else //if (ext == eExtension::_kdm)
                                title += ".kdm";
                            archive_entry_set_pathname(entry, title.c_str());
                            archive_entry_set_size(entry, mdatSize);
                            archive_entry_set_filetype(entry, AE_IFREG);
                            archive_entry_set_perm(entry, 0644);
                            archive_write_header(archive, entry);
                            Array<uint8_t> data(mdatSize);
                            file.Read(data.Items(), mdatSize);
                            archive_write_data(archive, data.Items(), mdatSize);

                            if (smplSize)
                            {
                                archive_entry_clear(entry);
                                title = songSheet->name.String();
                                io::File::CleanFilename(title.data());
                                if (ext == eExtension::_tfm || ext == eExtension::_mdst)
                                    title = "smpl." + title;
                                else if (ext == eExtension::_rjp || ext == eExtension::_dum || ext == eExtension::_sng)
                                    title += ".ins";
                                else if (ext == eExtension::_adsc)
                                    title += ".adsc.as";
                                else if (ext == eExtension::_kh)
                                    title = "songplay";
                                else if (ext == eExtension::_mcr)
                                    title = "mcs." + title;
                                else if (ext == eExtension::_pap || ext == eExtension::_qts || ext == eExtension::_osp)
                                    title = "smp.set";
                                else if (ext == eExtension::_pn)
                                    title = "pn." + title + ".info";
                                else if (ext == eExtension::_dat)
                                    title += ".ssd";
                                else if (ext == eExtension::_mod)
                                    title += ".mod.nt";
                                else if (ext == eExtension::_mus)
                                    title += ".str";
                                else if (ext == eExtension::_ksm)
                                    title = "insts.dat";
                                else if (ext == eExtension::_sci)
                                {
                                    if (title.size() > 3)
                                        title.resize(3);
                                    title += "patch.003";
                                }
                                else if (ext == eExtension::_rol)
                                    title = "standard.bnk";
                                else if (ext == eExtension::_kdm)
                                    title = "waves.kwv";
                                else //if (ext == eExtension::_tpu || ext == eExtension::_dns || ext == eExtension::_jpn || ext == eExtension::_mfp || ext == eExtension::_qts || ext == eExtension::_sjs || ext == eExtension::_sdr || ext == eExtension::_thm || ext == eExtension::_uds)
                                    title = "smp." + title;
                                archive_entry_set_pathname(entry, title.c_str());
                                archive_entry_set_size(entry, smplSize);
                                archive_entry_set_filetype(entry, AE_IFREG);
                                archive_entry_set_perm(entry, 0644);
                                archive_write_header(archive, entry);
                                data.Resize(smplSize);
                                file.Read(data.Items(), smplSize);
                                archive_write_data(archive, data.Items(), smplSize);
                            }

                            archive_write_free(archive);
                            archive_entry_free(entry);

                            if (ext == eExtension::_mdst)
                                archiveBuffer.Copy("MDST-MOD", sizeof("MDST-MOD") - 1);

                            file = {};
                            if (!io::File::Delete(oldFilename.c_str()))
                                Log::Warning("Can't delete file \"%s\"\n", oldFilename.c_str());

                            file = io::File::OpenForWrite(GetFullpath(song).c_str());
                            if (file.IsValid())
                            {
                                file.Write(archiveBuffer.Items(), archiveBuffer.NumItems());

                                songSheet->fileSize = archiveBuffer.Size<uint32_t>();
                                songSheet->fileCrc = crc32(0L, Z_NULL, 0);
                                songSheet->fileCrc = crc32_z(songSheet->fileCrc, archiveBuffer.Items(), archiveBuffer.Size<size_t>());
                            }
                        }
                    }
                    break;
                }
                default:
                    break;
                }
            }
            Raise(Database::Flag::kSaveSongs);
            m_hasFailedDeletes = true;
        }
    }
}
// namespace rePlayer