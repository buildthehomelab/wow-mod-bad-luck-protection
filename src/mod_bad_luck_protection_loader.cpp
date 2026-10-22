/*
 * mod-bad-luck-protection loader.
 *
 * AzerothCore looks up a loader symbol derived from the module's folder name: for folder
 * "mod-bad-luck-protection" that symbol is exactly "Addmod_bad_luck_protectionScripts". If you
 * clone the repo under a different folder name, rename this function to match.
 *
 * Released under the MIT License.
 */

void AddBadLuckScripts();
void AddBadLuckCommandScripts();

void Addmod_bad_luck_protectionScripts()
{
    AddBadLuckScripts();
    AddBadLuckCommandScripts();
}
