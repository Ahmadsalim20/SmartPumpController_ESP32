import { serve } from "https://deno.land/std@0.177.0/http/server.ts";
import { createClient } from "https://esm.sh/@supabase/supabase-js@2";

const corsHeaders = {
  "Access-Control-Allow-Origin": "*",
  "Access-Control-Allow-Headers": "authorization, x-client-info, apikey, content-type",
};

serve(async (req: Request) => {
  // Handle CORS preflight
  if (req.method === "OPTIONS") {
    return new Response("ok", { headers: corsHeaders });
  }

  try {
    const supabaseUrl = Deno.env.get("SUPABASE_URL") ?? "";
    const supabaseServiceKey =
      Deno.env.get("SUPABASE_SERVICE_ROLE_KEY") ??
      Deno.env.get("SUPABASE_SERVICE_KEY") ??
      "";

    if (!supabaseUrl || !supabaseServiceKey) {
      throw new Error("Missing SUPABASE_URL or SUPABASE_SERVICE_ROLE_KEY environment variables.");
    }

    const supabase = createClient(supabaseUrl, supabaseServiceKey);

    const body = await req.json();
    const { hardware_model_id, current_version, mac_address, status_report } = body;

    if (!hardware_model_id || !current_version) {
      return new Response(
        JSON.stringify({ error: "Missing required fields: hardware_model_id or current_version" }),
        { status: 400, headers: { ...corsHeaders, "Content-Type": "application/json" } }
      );
    }

    let deviceId: string | null = null;

    // 1. Register or update device status in 'devices' table if mac_address is provided
    if (mac_address) {
      const { data: existingDevice } = await supabase
        .from("devices")
        .select("id")
        .eq("mac_address", mac_address)
        .maybeSingle();

      if (existingDevice) {
        deviceId = existingDevice.id;
        await supabase
          .from("devices")
          .update({
            current_firmware_version: current_version,
            hardware_model_id: hardware_model_id,
            last_checkin: new Date().toISOString(),
            status: "online",
          })
          .eq("id", deviceId);
      } else {
        const { data: newDevice, error: insertDeviceErr } = await supabase
          .from("devices")
          .insert({
            mac_address: mac_address,
            hardware_model_id: hardware_model_id,
            current_firmware_version: current_version,
            last_checkin: new Date().toISOString(),
            status: "online",
          })
          .select("id")
          .single();

        if (!insertDeviceErr && newDevice) {
          deviceId = newDevice.id;
        }
      }
    }

    // 2. If this request is an OTA status report from the microcontroller
    if (status_report && status_report.target_firmware_id) {
      if (deviceId) {
        await supabase.from("ota_logs").insert({
          device_id: deviceId,
          target_firmware_id: status_report.target_firmware_id,
          status: status_report.status || "reported",
          error_message: status_report.error_message || null,
        });

        // If update was successful, ensure current_firmware_version is updated
        if (status_report.status === "success") {
          await supabase
            .from("devices")
            .update({
              current_firmware_version: current_version,
              last_checkin: new Date().toISOString(),
            })
            .eq("id", deviceId);
        }
      }

      return new Response(
        JSON.stringify({ success: true, message: "Status report recorded successfully" }),
        { status: 200, headers: { ...corsHeaders, "Content-Type": "application/json" } }
      );
    }

    // 3. Find latest active release for this hardware model
    const { data: releases, error: releaseErr } = await supabase
      .from("firmware_releases")
      .select("*")
      .eq("hardware_model_id", hardware_model_id)
      .eq("is_active", true)
      .order("created_at", { ascending: false });

    if (releaseErr || !releases || releases.length === 0) {
      return new Response(
        JSON.stringify({
          update_available: false,
          version: current_version,
          message: "No active firmware releases found for this hardware model.",
        }),
        { status: 200, headers: { ...corsHeaders, "Content-Type": "application/json" } }
      );
    }

    const latestRelease = releases[0];

    // Check if new version is different from current_version
    const isNewer = latestRelease.version !== current_version;

    if (!isNewer) {
      return new Response(
        JSON.stringify({
          update_available: false,
          version: current_version,
          message: "Device firmware is up to date.",
        }),
        { status: 200, headers: { ...corsHeaders, "Content-Type": "application/json" } }
      );
    }

    // 4. Generate download URL from Supabase Storage (firmware bucket)
    let downloadUrl = "";
    if (
      latestRelease.bin_file_path.startsWith("http://") ||
      latestRelease.bin_file_path.startsWith("https://")
    ) {
      downloadUrl = latestRelease.bin_file_path;
    } else {
      // Try generating signed URL first (expires in 1 hour)
      const { data: signedUrlData } = await supabase.storage
        .from("firmware")
        .createSignedUrl(latestRelease.bin_file_path, 3600);

      if (signedUrlData?.signedUrl) {
        downloadUrl = signedUrlData.signedUrl;
      } else {
        // Fallback to public URL if bucket is public
        const { data: publicUrlData } = supabase.storage
          .from("firmware")
          .getPublicUrl(latestRelease.bin_file_path);
        downloadUrl = publicUrlData.publicUrl;
      }
    }

    // 5. Log OTA offering event in 'ota_logs'
    if (deviceId) {
      await supabase.from("ota_logs").insert({
        device_id: deviceId,
        target_firmware_id: latestRelease.id,
        status: "offered",
      });
    }

    // 6. Return response to ESP device
    return new Response(
      JSON.stringify({
        update_available: true,
        firmware_id: latestRelease.id,
        version: latestRelease.version,
        download_url: downloadUrl,
        sha256_hash: latestRelease.sha256_hash,
        digital_signature: latestRelease.digital_signature,
        release_notes: latestRelease.release_notes || "",
      }),
      { status: 200, headers: { ...corsHeaders, "Content-Type": "application/json" } }
    );
  } catch (err: any) {
    return new Response(
      JSON.stringify({ error: err.message || "Internal server error" }),
      { status: 500, headers: { ...corsHeaders, "Content-Type": "application/json" } }
    );
  }
});
